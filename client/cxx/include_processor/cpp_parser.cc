// Copyright 2011 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "cpp_parser.h"

#include <limits.h>
#include <stdio.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <unordered_map>

#include "absl/memory/memory.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "compiler_info.h"
#include "compiler_specific.h"
#include "content.h"
#include "counterz.h"
#include "cpp_directive_parser.h"
#include "cpp_input.h"
#include "cpp_integer_constant_evaluator.h"
#include "cpp_macro.h"
#include "cpp_macro_expander.h"
#include "cpp_tokenizer.h"
#include "ioutil.h"
#include "lockhelper.h"
#include "path.h"
#include "path_resolver.h"
#include "util.h"

namespace {

static const int kIncludeFileDepthLimit = 1024;

}  // anonymous namespace

namespace devtools_goma {

const int CppParser::kCurrentDirIncludeDirIndex;
const int CppParser::kIncludeDirIndexStarting;

// CppParser::PragmaOnceFileSet ----------------------------------------

void CppParser::PragmaOnceFileSet::Insert(const std::string& file) {
  files_.insert(PathResolver::ResolvePath(file));
}

bool CppParser::PragmaOnceFileSet::Has(const std::string& file) const {
  if (files_.empty()) {
    return false;
  }
  return files_.contains(PathResolver::ResolvePath(file));
}

// CppParser -----------------------------------------------------------

bool CppParser::global_initialized_ = false;
CppParser::PredefinedMacros* CppParser::predefined_macros_ = nullptr;

CppParser::CppParser()
    : condition_in_false_depth_(0),
      counter_(0),
      is_cplusplus_(false),
      bracket_include_dir_index_(kIncludeDirIndexStarting),
      include_observer_(nullptr),
      error_observer_(nullptr),
      compiler_info_(nullptr),
      is_vc_(false),
      disabled_(false),
      skipped_files_(0),
      total_files_(0),
      owner_thread_id_(GetCurrentThreadId()) {
  const absl::Time now = absl::Now();
  current_time_ = absl::FormatTime("%H:%M:%S", now, absl::LocalTimeZone());
  current_date_ = absl::FormatTime("%b %d %Y", now, absl::LocalTimeZone());
  EnsureInitialize();

  // Push empty input as a sentinel.
  SharedCppDirectives directives(
      CppDirectiveParser::ParseFromString("", "<empty>"));
  last_input_ = absl::make_unique<CppInput>(directives.get(), "", "<empty>",
                                            "<empty>", -1, 0);
  input_protects_.push_back(std::move(directives));
}

CppParser::~CppParser() {
  DCHECK(THREAD_ID_IS_SELF(owner_thread_id_));
  while (!inputs_.empty())
    PopInput();
}

void CppParser::SetCompilerInfo(const CxxCompilerInfo* compiler_info) {
  compiler_info_ = compiler_info;
  if (compiler_info_ == nullptr)
    return;

  // is_cplusplus if c++ / objective-c++ / c++-header
  // see GCCFlags::GetLanguage
  // http://b/167131595
  set_is_cplusplus(absl::StrContains(compiler_info_->lang(), "c++"));
  SetTarget(compiler_info_->cxx_target());

  AddPredefinedMacros(*compiler_info);
  AddPreparsedDirectivesInput(compiler_info->predefined_directives());
  ProcessDirectives();
}

bool CppParser::ProcessDirectives() {
  GOMA_COUNTERZ("ProcessDirectives");
  if (disabled_)
    return false;

  while (const CppDirective* directive = NextDirective()) {
    VLOG(2) << DebugStringPrefix()
            << " Directive:" << directive->DirectiveTypeName();
    if (CurrentCondition()) {
      ProcessDirective(*directive);
    } else {
      ProcessDirectiveInFalseCondition(*directive);
    }
  }

  return !disabled_;
}

void CppParser::ProcessDirective(const CppDirective& d) {
  switch (d.type()) {
    case CppDirectiveType::DIRECTIVE_INCLUDE:
      ProcessInclude(AsCppDirectiveInclude(d));
      return;
    case CppDirectiveType::DIRECTIVE_IMPORT:
      ProcessImport(AsCppDirectiveImport(d));
      return;
    case CppDirectiveType::DIRECTIVE_INCLUDE_NEXT:
      ProcessIncludeNext(AsCppDirectiveIncludeNext(d));
      return;
    case CppDirectiveType::DIRECTIVE_DEFINE:
      ProcessDefine(AsCppDirectiveDefine(d));
      return;
    case CppDirectiveType::DIRECTIVE_UNDEF:
      ProcessUndef(AsCppDirectiveUndef(d));
      return;
    case CppDirectiveType::DIRECTIVE_IFDEF:
      ProcessIfdef(AsCppDirectiveIfdef(d));
      return;
    case CppDirectiveType::DIRECTIVE_IFNDEF:
      ProcessIfndef(AsCppDirectiveIfndef(d));
      return;
    case CppDirectiveType::DIRECTIVE_IF:
      ProcessIf(AsCppDirectiveIf(d));
      return;
    case CppDirectiveType::DIRECTIVE_ELSE:
      ProcessElse(AsCppDirectiveElse(d));
      return;
    case CppDirectiveType::DIRECTIVE_ENDIF:
      ProcessEndif(AsCppDirectiveEndif(d));
      return;
    case CppDirectiveType::DIRECTIVE_ELIF:
      ProcessElif(AsCppDirectiveElif(d));
      return;
    case CppDirectiveType::DIRECTIVE_PRAGMA:
      ProcessPragma(AsCppDirectivePragma(d));
      return;
    case CppDirectiveType::DIRECTIVE_ERROR:
      ProcessError(AsCppDirectiveError(d));
      return;
      // no default: to detect case is exhaustive.
  }

  CHECK(false) << "unknown directive type";
}

void CppParser::ProcessDirectiveInFalseCondition(const CppDirective& d) {
  switch (d.type()) {
    case CppDirectiveType::DIRECTIVE_IFDEF:
      ProcessConditionInFalse(d);
      return;
    case CppDirectiveType::DIRECTIVE_IFNDEF:
      ProcessConditionInFalse(d);
      return;
    case CppDirectiveType::DIRECTIVE_IF:
      ProcessConditionInFalse(d);
      return;
    case CppDirectiveType::DIRECTIVE_ELSE:
      ProcessElse(AsCppDirectiveElse(d));
      return;
    case CppDirectiveType::DIRECTIVE_ENDIF:
      ProcessEndif(AsCppDirectiveEndif(d));
      return;
    case CppDirectiveType::DIRECTIVE_ELIF:
      ProcessElif(AsCppDirectiveElif(d));
      return;
    default:
      // do nothing
      return;
  }
}

const CppDirective* CppParser::NextDirective() {
  while (HasMoreInput()) {
    if (const CppDirective* directive = input()->NextDirective()) {
      return directive;
    }

    PopInput();
  }
  return nullptr;
}

void CppParser::AddMacroByString(const std::string& name,
                                 const std::string& body) {
  std::string macro =
      "#define " + name + (body.empty() ? "" : " ") + body + '\n';
  AddStringInput(macro, "<macro>");
  ProcessDirectives();
}

void CppParser::AddMacro(const Macro* macro) {
  const Macro* existing_macro = macro_env_.Add(macro);
  if (existing_macro) {
    if (existing_macro->IsPredefinedMacro()) {
      Error("redefining predefined macro ", existing_macro->name);
    } else {
      Error("macro is already defined:", existing_macro->name);
    }
  } else {
    VLOG(1) << "#DEFINE " << macro->DebugString(this);
  }
}

const Macro* CppParser::GetMacro(const std::string& name) {
  return macro_env_.Get(name);
}

void CppParser::DeleteMacro(const std::string& name) {
  const Macro* existing_macro = macro_env_.Delete(name);

  if (existing_macro && existing_macro->IsPredefinedMacro()) {
    Error("predefined macro is deleted:", name);
  }
}

bool CppParser::IsMacroDefined(const std::string& name) {
  const Macro* m = GetMacro(name);
  if (!m) {
    return false;
  }

  // Hack for GCC 5.
  // e.g. __has_include__ is not defined but callable.
  if (m->is_hidden) {
    return false;
  }

  return true;
}

bool CppParser::EnablePredefinedMacro(const std::string& name, bool is_hidden) {
  for (const auto& p : *predefined_macros_) {
    if (p.first == name && p.second->is_hidden == is_hidden) {
      const Macro* existing = macro_env_.Add(p.second.get());
      return existing == nullptr;
    }
  }

  // Nothing matched
  return false;
}

void CppParser::AddStringInput(const std::string& content,
                               const std::string& pathname) {
  if (inputs_.size() >= kIncludeFileDepthLimit) {
    LOG(ERROR) << "Exceed include depth limit: " << kIncludeFileDepthLimit
               << " pathname: " << pathname;
    disabled_ = true;
    return;
  }

  SharedCppDirectives directives =
      CppDirectiveParser().ParseFromString(content, pathname);
  if (!directives) {
    LOG(ERROR) << "failed to parse: " << content << " pathname: " << pathname;
    disabled_ = true;
    return;
  }

  // need to protect.
  inputs_.emplace_back(new Input(directives.get(), "", pathname, "(string)",
                                 kCurrentDirIncludeDirIndex, (directives->size() > 0) ? directives->front()->line_in_file() : 0));
  input_protects_.push_back(std::move(directives));
}

void CppParser::AddPredefinedMacros(const CxxCompilerInfo& compiler_info) {
  // predefined_macros_ has `hidden` pattern and non-`hidden` pattern.
  // We need to check is_hidden, too.
  for (const auto& p : *predefined_macros_) {
    const std::string& name = p.first;
    const Macro* macro = p.second.get();
    auto it = compiler_info.supported_predefined_macros().find(name);
    if (it == compiler_info.supported_predefined_macros().end()) {
      continue;
    }
    if (it->second == macro->is_hidden) {
      // found. we need to insert this.
      const Macro* existing = macro_env_.Add(macro);
      if (existing != nullptr) {
        LOG(ERROR) << "The same name predefined macro detected: "
                   << existing->name;
      }
    }
  }
}

void CppParser::AddFileInput(IncludeItem include_item,
                             const std::string& filepath,
                             const std::string& directory,
                             int include_dir_index) {
  if (inputs_.size() >= kIncludeFileDepthLimit) {
    LOG(ERROR) << "Exceeds include depth limit: " << kIncludeFileDepthLimit
               << " filepath: " << filepath;
    disabled_ = true;
    return;
  }

  DCHECK_GE(include_dir_index, kCurrentDirIncludeDirIndex);
  if (base_file_.empty())
    base_file_ = filepath;

  inputs_.emplace_back(new Input(include_item.directives().get(),
                                 include_item.include_guard_ident(), filepath,
                                 directory, include_dir_index, (include_item.directives()->size() > 0) ? include_item.directives()->front()->line_in_file() : 0));
  input_protects_.push_back(include_item.directives());
  VLOG(2) << "Including file: " << filepath;
}

void CppParser::AddPreparsedDirectivesInput(SharedCppDirectives directives) {
  CHECK(directives);
  inputs_.emplace_back(new Input(directives.get(), "", "<preparsed>",
                                 "<preparsed>", kCurrentDirIncludeDirIndex, (directives->size() > 0) ? directives->front()->line_in_file() : 0));
  input_protects_.push_back(std::move(directives));
}

std::string CppParser::DumpMacros() {
  std::stringstream ss;
  for (const auto& entry : macro_env_.UnderlyingMap()) {
    ss << entry.second->DebugString(this) << std::endl;
  }
  return ss.str();
}

std::string CppParser::DebugStringPrefix() {
  std::string str;
  str.reserve(input()->filepath().size() + 32);
  str.append("(");
  str.append(input()->filepath());
  str.append(":");
  // TODO: This is not line position.
  if (input()->CurrentDirective() != nullptr) {
    str.append(std::to_string(input()->CurrentDirective()->line_in_file()));
  } else {
    str.append(std::to_string(input()->line_in_file()));
  }
  str.append(")");
  return str;
}

void CppParser::Error(absl::string_view error) {
  if (!error_observer_)
    return;
  Error(error, "");
}

void CppParser::Error(absl::string_view error, absl::string_view arg) {
  if (!error_observer_)
    return;
  std::string str;
  str.reserve(error.size() + input()->filepath().size() + 100);
  str.append("CppParser");
  str.append(DebugStringPrefix());
  str.append(" ");
  absl::StrAppend(&str, error, arg);
  error_observer_->HandleError(str);
}

void CppParser::ProcessInclude(const CppDirectiveInclude& d) {
  GOMA_COUNTERZ("include");
  ProcessIncludeInternal(d);
}

void CppParser::ProcessImport(const CppDirectiveImport& d) {
  GOMA_COUNTERZ("import");
  if (!is_vc_) {
    // For gcc, #import means include only-once.
    // http://gcc.gnu.org/onlinedocs/gcc-3.2/cpp/Obsolete-once-only-headers.html

    // For Objective-C, #import means include only-once.
    // https://developer.apple.com/library/mac/documentation/MacOSX/Conceptual/BPFrameworks/Tasks/IncludingFrameworks.html
    // > If you are working in Objective-C, you may use the #import directive
    // instead of the #include directive. The two directives have the same
    // basic results. but the #import directive guarantees that the same
    // header file is never included more than once.
    ProcessIncludeInternal(d);
    return;
  }
  // For VC++, #import is used to incorporate information from a type library.
  // http://msdn.microsoft.com/en-us/library/8etzzkb6(v=vs.71).aspx
  LOG(WARNING) << DebugStringPrefix() << " #import used, "
               << "but goma couldn't handle it yet. "
               << "See b/9286087";
  disabled_ = true;
}

void CppParser::ProcessIncludeNext(const CppDirectiveIncludeNext& d) {
  GOMA_COUNTERZ("include_next");
  ProcessIncludeInternal(d);
}

void CppParser::ProcessDefine(const CppDirectiveDefine& d) {
  GOMA_COUNTERZ("define");
  AddMacro(d.macro());
}

void CppParser::ProcessUndef(const CppDirectiveUndef& d) {
  GOMA_COUNTERZ("undef");
  DeleteMacro(d.name());
}

void CppParser::ProcessConditionInFalse(const CppDirective& directive) {
  ++condition_in_false_depth_;
}

void CppParser::ProcessIfdef(const CppDirectiveIfdef& d) {
  GOMA_COUNTERZ("ifdef");
  bool v = IsMacroDefined(d.name());
  VLOG(2) << DebugStringPrefix() << " #IFDEF " << v;
  conditions_.push_back(Condition(v));
}

void CppParser::ProcessIfndef(const CppDirectiveIfndef& d) {
  GOMA_COUNTERZ("ifndef");
  bool v = !IsMacroDefined(d.name());
  VLOG(2) << DebugStringPrefix() << " #IFNDEF " << v;
  conditions_.push_back(Condition(v));
}

void CppParser::ProcessIf(const CppDirectiveIf& d) {
  GOMA_COUNTERZ("if");
  int64_t v = EvalCondition(d.tokens());
  VLOG(2) << DebugStringPrefix() << " #IF " << v;
  conditions_.push_back(Condition(v != 0));
}

void CppParser::ProcessElse(const CppDirectiveElse& d) {
  GOMA_COUNTERZ("else");
  if (condition_in_false_depth_ > 0) {
    return;
  }
  if (conditions_.empty()) {
    Error("stray else");
    return;
  }
  conditions_.back().cond = (!conditions_.back().cond &&
                             !conditions_.back().taken);
}

void CppParser::ProcessEndif(const CppDirectiveEndif& d) {
  GOMA_COUNTERZ("endif");
  if (condition_in_false_depth_) {
    --condition_in_false_depth_;
    return;
  }
  if (conditions_.empty()) {
    Error("stray endif");
    return;
  }
  conditions_.pop_back();
}

void CppParser::ProcessElif(const CppDirectiveElif& d) {
  GOMA_COUNTERZ("elif");
  if (condition_in_false_depth_ > 0) {
    return;
  }
  if (conditions_.empty()) {
    Error("stray elif");
    return;
  }
  if (conditions_.back().taken) {
    conditions_.back().cond = false;
    return;
  }

  int64_t v = EvalCondition(d.tokens());
  VLOG(2) << DebugStringPrefix() << " #ELIF " << v;
  conditions_.back().cond = (v != 0);
  conditions_.back().taken |= (v != 0);
}

void CppParser::ProcessPragma(const CppDirectivePragma& d) {
  GOMA_COUNTERZ("pragma");

  if (d.is_pragma_once()) {
    pragma_once_fileset_.Insert(input()->filepath());
  }
}

void CppParser::ProcessError(const CppDirectiveError& d) {
  Error(d.error_reason(), d.arg());
}

void CppParser::ProcessIncludeInternal(const CppDirectiveIncludeBase& d) {
  // Simple <filepath> case.
  if (d.delimiter() == '<') {
    const std::string& path = d.filename();
    if (!path.empty() && include_observer_) {
      int next_index = bracket_include_dir_index_;
      if (d.type() == CppDirectiveType::DIRECTIVE_INCLUDE_NEXT) {
        next_index = input()->include_dir_index() + 1;
      }
      // We should not find the current directory (without specifying by -I).
      DCHECK_GE(next_index, bracket_include_dir_index_)
          << ' ' << input()->include_dir_index();
      if (!include_observer_->HandleInclude(
              path, input()->directory(), input()->filepath(), '<',
              next_index)) {
        LOG(WARNING) << "HandleInclude failed #" << d.DirectiveTypeName()
                     << " <" << path << ">"
                     << " from " << input()->filepath()
                     << " [dir:" << input()->directory()
                     << " index:" << input()->include_dir_index() << "]";
        disabled_ = true;
        return;
      }
      if (d.type() == CppDirectiveType::DIRECTIVE_IMPORT) {
        DCHECK(!inputs_.empty());
        const std::string& filepath = inputs_.back()->filepath();
        pragma_once_fileset_.Insert(filepath);
        VLOG(1) << "HandleInclude #import " << filepath;
      }
    }
    return;
  }

  // Simple "filepath" case
  if (d.delimiter() == '"') {
    const std::string& path = d.filename();
    if (!path.empty() && include_observer_) {
      int quote_char = '"';
      int next_index = input()->include_dir_index();
      if (d.type() == CppDirectiveType::DIRECTIVE_INCLUDE_NEXT) {
        quote_char = '<';
        ++next_index;
      }
      if (!include_observer_->HandleInclude(
              path, input()->directory(), input()->filepath(), quote_char,
              next_index)) {
        LOG(WARNING) << "HandleInclude failed #" << d.DirectiveTypeName()
                     << " \"" << path << "\""
                     << " from " << input()->filepath()
                     << " [dir:" << input()->directory()
                     << " index:" << input()->include_dir_index() << "]";
        disabled_ = true;
        return;
      }
      if (d.type() == CppDirectiveType::DIRECTIVE_IMPORT) {
        DCHECK(!inputs_.empty());
        const std::string& filepath = inputs_.back()->filepath();
        pragma_once_fileset_.Insert(filepath);
        VLOG(1) << "HandleInclude #import " << filepath;
      }
    }
    return;
  }

  DCHECK_EQ(' ', d.delimiter());

  ArrayTokenList expanded =
      CppMacroExpander(this).Expand(d.tokens(), SpaceHandling::kKeep);

  if (expanded.empty()) {
    Error("#include expects \"filename\" or <filename>");
    LOG(WARNING) << "HandleInclude empty arg for #" << d.DirectiveTypeName();
    return;
  }

  // See if the expanded token(s) is <filepath> or "filepath".
  CppToken token = expanded.front();
  if (token.type == Token::LT) {
    std::string path;
    auto iter = expanded.begin();
    ++iter;
    for (; iter != expanded.end() && iter->type != Token::GT; ++iter) {
      path.append(iter->GetCanonicalString());
    }
    int next_index = bracket_include_dir_index_;
    if (d.type() == CppDirectiveType::DIRECTIVE_INCLUDE_NEXT) {
      next_index = input()->include_dir_index() + 1;
      DCHECK_GE(next_index, bracket_include_dir_index_);
    }
    if (include_observer_) {
      if (!include_observer_->HandleInclude(
              path, input()->directory(), input()->filepath(), '<',
              next_index)) {
        LOG(WARNING) << "HandleInclude failed #" << d.DirectiveTypeName()
                     << " <" << path << ">"
                     << " from " << input()->filepath()
                     << " [dir:" << input()->directory()
                     << " index:" << input()->include_dir_index() << "]";
        disabled_ = true;
        return;
      }
      if (d.type() == CppDirectiveType::DIRECTIVE_IMPORT) {
        DCHECK(!inputs_.empty());
        const std::string& filepath = inputs_.back()->filepath();
        pragma_once_fileset_.Insert(filepath);
        VLOG(1) << "HandleInclude #import " << filepath;
      }
    }
    return;
  }
  if (token.type == Token::STRING) {
    if (include_observer_) {
      int quote_char = '"';
      int next_index = input()->include_dir_index();
      if (d.type() == CppDirectiveType::DIRECTIVE_INCLUDE_NEXT) {
        quote_char = '<';
        ++next_index;
      }
      if (!include_observer_->HandleInclude(
              token.string_value, input()->directory(), input()->filepath(),
              quote_char, next_index)) {
        LOG(WARNING) << "HandleInclude failed #" << d.DirectiveTypeName()
                     << " \"" << token.string_value << "\""
                     << " from " << input()->filepath()
                     << " [dir:" << input()->directory()
                     << " index:" << input()->include_dir_index() << "]";
        disabled_ = true;
        return;
      }
      if (d.type() == CppDirectiveType::DIRECTIVE_IMPORT) {
        DCHECK(!inputs_.empty());
        const std::string& filepath = inputs_.back()->filepath();
        pragma_once_fileset_.Insert(filepath);
        VLOG(1) << "HandleInclude #import " << filepath;
      }
    }
    return;
  }
  Error("#include expects \"filename\" or <filename>");
}

int64_t CppParser::EvalCondition(const ArrayTokenList& orig_tokens) {
  // TODO: Add DCHECK here orig_tokens does not contain spaces.
  ArrayTokenList tokens;
  tokens.reserve(orig_tokens.size());

  // convert "[defined][(][xxx][)] or [defined][xxx]
  // We need to convert defined() in #if here due to b/6533195.
  for (size_t i = 0; i < orig_tokens.size(); ++i) {
    if (orig_tokens[i].type == Token::IDENTIFIER &&
        orig_tokens[i].string_value == "defined") {
      if (i + 1 < orig_tokens.size() &&
          orig_tokens[i + 1].type == CppToken::IDENTIFIER) {
        int defined = IsMacroDefined(orig_tokens[i + 1].string_value);
        tokens.push_back(Token(defined));
        i += 1;
        continue;
      }

      if (i + 3 < orig_tokens.size() && orig_tokens[i + 1].IsPuncChar('(') &&
          orig_tokens[i + 2].type == CppToken::IDENTIFIER &&
          orig_tokens[i + 3].IsPuncChar(')')) {
        int defined = IsMacroDefined(orig_tokens[i + 2].string_value);
        tokens.push_back(Token(defined));
        i += 3;
        continue;
      }

      // unexpected defined. fallthrough.
    }

    tokens.push_back(orig_tokens[i]);
  }

  // 2. Expands macros.
  ArrayTokenList expanded =
      CppMacroExpander(this).Expand(tokens, SpaceHandling::kSkip);

  // 3. Evaluates the expanded integer constant expression.
  return CppIntegerConstantEvaluator(expanded, this).GetValue();
}

void CppParser::PopInput() {
  DCHECK(HasMoreInput());

  std::unique_ptr<Input> current = std::move(inputs_.back());
  inputs_.pop_back();

  if (!current->filepath().empty() && !current->include_guard_ident().empty() &&
      IsMacroDefined(current->include_guard_ident())) {
    include_guard_ident_[current->filepath()] = current->include_guard_ident();
  }

  last_input_ = std::move(current);
}

bool CppParser::IsProcessedFileInternal(const std::string& path,
                                        int include_dir_index) {
  VLOG(2) << "IsProcessedFileInternal:"
          << " path=" << path
          << " include_dir_index=" << include_dir_index;
  // Check if this file is in the pragma_once history.
  if (pragma_once_fileset_.Has(path)) {
    VLOG(1) << "Skipping " << path << " for pragma once";
    return true;
  }

  const auto& iter = include_guard_ident_.find(path);
  if (iter == include_guard_ident_.end()) {
    return false;
  }
  if (IsMacroDefined(iter->second)) {
    VLOG(1) << "Skipping " << path << " for include guarded by "
            << iter->second;
    return true;
  }
  return false;
}

CppParser::Token CppParser::GetFileName() {
  Token token(Token::STRING);
  token.Append(input()->filepath());
  return token;
}

CppParser::Token CppParser::GetLineNumber() {
  Token token(Token::NUMBER);
  token.v.int_value = input()->directive_pos();
  token.Append(std::to_string(token.v.int_value));
  return token;
}

CppParser::Token CppParser::GetDate() {
  Token token(Token::STRING);
  token.Append(current_date_);
  return token;
}

CppParser::Token CppParser::GetTime() {
  Token token(Token::STRING);
  token.Append(current_time_);
  return token;
}

CppParser::Token CppParser::GetCounter() {
  return Token(counter_++);
}

CppParser::Token CppParser::GetBaseFile() {
  Token token(Token::STRING);
  token.Append(base_file_);
  return token;
}

CppParser::Token CppParser::ProcessHasInclude(const ArrayTokenList& tokens) {
  return Token(static_cast<int>(ProcessHasIncludeInternal(tokens, false)));
}

CppParser::Token CppParser::ProcessHasIncludeNext(
    const ArrayTokenList& tokens) {
  return Token(static_cast<int>(ProcessHasIncludeInternal(tokens, true)));
}

bool CppParser::ProcessHasIncludeInternal(const ArrayTokenList& tokens,
                                          bool is_include_next) {
  GOMA_COUNTERZ("ProcessHasIncludeInternal");
  if (tokens.empty()) {
    Error("__has_include expects \"filename\" or <filename>");
    return false;
  }
  VLOG(1) << "has_include front=" << tokens.front()
          << " type=" << tokens.front().type;
  ArrayTokenList expanded;
  // Simple <filepath> case.
  if (tokens.front().type == Token::LT) {
    VLOG(1) << "simple <filenpath> case";
    expanded = tokens;
  } else if (tokens.front().type == Token::STRING) {
    VLOG(1) << "simple \"filenpath\" case";
    expanded = tokens;
  } else {
    // TODO: need this? caller already expanded args? crbug.com/1386100
    VLOG(1) << "expand has_include ";
    ArrayTokenList tokenlist(tokens.begin(), tokens.end());
    expanded = CppMacroExpander(this).Expand(tokenlist, SpaceHandling::kKeep);
  }
  if (expanded.empty()) {
    Error("__has_include expects \"filename\" or <filename>");
    return false;
  }

  Token token = expanded.front();
  if (token.type == Token::LT) {
    std::string path;
    auto iter = expanded.begin();
    ++iter;
    for (; iter != expanded.end() && iter->type != Token::GT; ++iter) {
      path.append(iter->GetCanonicalString());
    }
    VLOG(1) << DebugStringPrefix() << "HAS_INCLUDE(<" << path << ">)";
    if (include_observer_) {
      return include_observer_->HasInclude(
          path, input()->directory(), input()->filepath(),
          '<',
          is_include_next ? (input()->include_dir_index() + 1) :
          bracket_include_dir_index_);
    }
    return false;
  }
  if (token.type == Token::STRING) {
    VLOG(1) << DebugStringPrefix() << "HAS_INCLUDE(" << token.string_value
            << ")";
    if (include_observer_) {
      return include_observer_->HasInclude(
          token.string_value, input()->directory(), input()->filepath(),
          is_include_next ? '<' : '"',
          is_include_next ? (input()->include_dir_index() + 1) :
          input()->include_dir_index());
    }
    return false;
  }
  Error("__has_include expects \"filename\" or <filename>");
  return false;
}

CppParser::Token CppParser::ProcessHasCheckMacro(
    const std::string& name,
    const ArrayTokenList& tokens,
    const absl::flat_hash_map<std::string, int>& has_check_macro) {
  GOMA_COUNTERZ("ProcessHasCheckMacro");

  if (tokens.empty()) {
    Error(name + " expects an identifier");
    VLOG(1) << "ProcessHasCheckMacro " << name << ": expects an identifier";
    return Token(0);
  }

  ArrayTokenList expanded =
      CppMacroExpander(this).Expand(tokens, SpaceHandling::kSkip);
  if (expanded.empty()) {
    Error(name + " expects an identifier");
    VLOG(1) << "ProcessHasCheckMacro " << name
            << ": expects an identifier (expanded)";
    return Token(0);
  }

  // Let's consider "__has_cpp_attribute(clang::fallthrough)".
  // Here, token list is like "clang" ":" ":" "fallthrough".
  //
  // TODO: what happens
  //   1. if space is inserted between tokens?
  //   2. if clang or fallthrough is defined somwhere?
  //
  // b/71611716

  std::string ident;
  if (expanded.size() > 1) {
    // Concat the expanded tokens. Allow only ident or ':'.
    for (const auto& t : expanded) {
      if (t.type == Token::IDENTIFIER) {
        ident += t.string_value;
      } else if (t.IsPuncChar(':')) {
        ident += ':';
      } else {
        Error(name + " expects an identifier");
        VLOG(1) << "ProcessHasCheckMacro " << name
                << ": expects an identifier (allow :)";
        return Token(0);
      }
    }
  } else {
    Token token = expanded.front();
    CppToken::Type token_type = Token::IDENTIFIER;
    absl::string_view token_type_name = "an identifier";
    if (name == "__has_warning") {
      // __has_warning takes string, not identifier.
      // e.g. #if __has_warning("-Wformat-insufficient-args")
      token_type = Token::STRING;
      token_type_name = "a string";
    }
    if (token.type != token_type) {
      Error(absl::StrCat(name, " expects ", token_type_name));
      VLOG(1) << "ProcessHasCheckMacro " << name << ": expects "
              << token_type_name << " " << token;
      return Token(0);
    }
    ident = token.string_value;
  }

  // Normalize the extension identifier.
  // '__feature__' is normalized to 'feature' in clang.
  if (ident.size() >= 4 && absl::StartsWith(ident, "__")
      && absl::EndsWith(ident, "__")) {
    ident.resize(ident.size() - 2);
    ident = ident.substr(2);
  }

  const auto& iter = has_check_macro.find(ident);
  if (iter == has_check_macro.end()) {
    VLOG(1) << "ProcessHasCheckMacro " << name << " ident=" << ident
            << " not found";
    return Token(0);
  }
  VLOG(1) << "ProcessHasCheckMacro " << name << " ident=" << ident << "=>"
          << iter->second;
  return Token(iter->second);
}

void CppParser::SetTarget(absl::string_view target) {
  // llvm/lib/Support/Triple.cpp
  std::vector<std::string> tokens = absl::StrSplit(target, "-");
  if (tokens.size() > 0) {
    // normalize as parseArch in Triple.cpp?
    target_.arch = tokens[0];
  }
  if (tokens.size() > 1) {
    // normalize as parseVendor in Triple.cpp?
    target_.vendor = tokens[1];
  }
  if (tokens.size() > 2) {
    // normalize as parseOS in Triple.cpp?
    if (absl::StartsWith(tokens[2], "ios")) {
      target_.os = "ios";
    } else if (absl::StartsWith(tokens[2], "apple")) {
      target_.os = "apple";
    } else {
      target_.os = tokens[2];
    }
  }
  if (tokens.size() > 3) {
    // normalize as parseEnvironment in Triple.cpp?
    target_.environment = tokens[3];
  }
  VLOG(1) << "target: arch=" << target_.arch << " vendor=" << target_.vendor
          << " os=" << target_.os << " env=" << target_.environment;
}

CppParser::Token CppParser::ProcessIsTargetArch(const ArrayTokenList& tokens) {
  return ProcessIsTarget("__is_target_arch", tokens, target_.arch);
}

CppParser::Token CppParser::ProcessIsTargetVendor(
    const ArrayTokenList& tokens) {
  return ProcessIsTarget("__is_target_vendor", tokens, target_.vendor);
}

CppParser::Token CppParser::ProcessIsTargetOS(const ArrayTokenList& tokens) {
  // Darwin matches macos, ios, etc
  // https://github.com/llvm/llvm-project/blob/3542168be0a0f3bb88b5298e4463c185a69f0357/clang/lib/Lex/PPMacroExpansion.cpp#L1438-L1441
  if (absl::StartsWith(target_.os, "darwin")) {
    Token token = MacroParamToken(tokens);
    // Should it also support WatchOS, TVOS?
    return Token(absl::StartsWith(token.string_value, "darwin") ||
                 absl::StartsWith(token.string_value, "macos") ||
                 absl::StartsWith(token.string_value, "ios"));
  }
  return ProcessIsTarget("__is_target_os", tokens, target_.os);
}

CppParser::Token CppParser::ProcessIsTargetEnvironment(
    const ArrayTokenList& tokens) {
  return ProcessIsTarget("__is_target_environment", tokens,
                         target_.environment);
}

CppParser::Token CppParser::ProcessIsTarget(const std::string& name,
                                            const ArrayTokenList& tokens,
                                            const std::string& target_part) {
  Token token = MacroParamToken(tokens);
  if (token.type != Token::IDENTIFIER) {
    Error(name + " expects an identifier");
    VLOG(1) << name << " expects an identifier";
    return Token(0);
  }
  VLOG(1) << name << " " << token.string_value << " " << target_part;
  return Token(token.string_value == target_part);
}

CppParser::Token CppParser::MacroParamToken(const ArrayTokenList& tokens) {
  if (tokens.empty()) {
    return Token(0);
  }
  std::string ident;
  if (tokens.size() > 1) {
    for (const auto& t : tokens) {
      if (t.type == Token::IDENTIFIER) {
        ident += t.string_value;
      } else if (t.IsPuncChar(':')) {
        ident += ':';
      } else {
        return Token(0);
      }
    }
  } else {
    Token token = tokens.front();
    if (token.type != Token::IDENTIFIER) {
      return Token(0);
    }
    ident = token.string_value;
  }
  return Token(Token::IDENTIFIER, ident);
}

// static
void CppParser::EnsureInitialize() {
  static absl::once_flag key_once;
  absl::call_once(key_once, InitializeStaticOnce);
}

// static
void CppParser::InitializeStaticOnce() {
  DCHECK(!global_initialized_);

  predefined_macros_ = new PredefinedMacros;

  typedef CppParser self;
  static const struct {
    const char* name;
    Macro::CallbackObj callback;
  } kPredefinedCallbackMacros[] = {
    { "__FILE__", &self::GetFileName },
    { "__LINE__", &self::GetLineNumber },
    { "__DATE__", &self::GetDate },
    { "__TIME__", &self::GetTime },
    { "__COUNTER__",   &self::GetCounter },
    { "__BASE_FILE__", &self::GetBaseFile },
  };
  for (const auto& iter : kPredefinedCallbackMacros) {
    auto macro = absl::make_unique<Macro>(iter.name, Macro::CBK, iter.callback);
    predefined_macros_->emplace_back(iter.name, std::move(macro));
  }

  static const struct {
    const char* name;
    Macro::CallbackFunc callback;
  } kPredefinedCallbackFuncMacros[] = {
      {"__has_include", &self::ProcessHasInclude},
      {"__has_include__", &self::ProcessHasInclude},
      {"__has_include_next", &self::ProcessHasIncludeNext},
      {"__has_include_next__", &self::ProcessHasIncludeNext},
      {"__has_feature", &self::ProcessHasFeature},
      {"__has_extension", &self::ProcessHasExtension},
      {"__has_attribute", &self::ProcessHasAttribute},
      {"__has_cpp_attribute", &self::ProcessHasCppAttribute},
      {"__has_declspec_attribute", &self::ProcessHasDeclspecAttribute},
      {"__has_builtin", &self::ProcessHasBuiltin},
      {"__has_warning", &self::ProcessHasWarning},
      {"__is_target_arch", &self::ProcessIsTargetArch},
      {"__is_target_vendor", &self::ProcessIsTargetVendor},
      {"__is_target_os", &self::ProcessIsTargetOS},
      {"__is_target_environment", &self::ProcessIsTargetEnvironment},
  };

  for (const auto& iter : kPredefinedCallbackFuncMacros) {
    // For non hidden macro.
    auto m1(absl::make_unique<Macro>(iter.name, Macro::CBK_FUNC, iter.callback,
                                     false));
    predefined_macros_->emplace_back(iter.name, std::move(m1));
    // For hidden macro
    auto m2(absl::make_unique<Macro>(iter.name, Macro::CBK_FUNC, iter.callback,
                                     true));
    predefined_macros_->emplace_back(iter.name, std::move(m2));
  }

  global_initialized_ = true;
}

}  // namespace devtools_goma
