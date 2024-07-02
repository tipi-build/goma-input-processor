// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cpp_integer_constant_evaluator.h"

namespace devtools_goma {

CppIntegerConstantEvaluator::CppIntegerConstantEvaluator(
    const ArrayTokenList& tokens,
    CppParser* parser)
    : tokens_(tokens), iter_(tokens.begin()), parser_(parser) {
  CHECK(parser_);
  VLOG(2) << parser_->DebugStringPrefix() << " Evaluating: "
          << DebugString(TokenList(tokens.begin(), tokens.end()));
}

CppToken::int_value CppIntegerConstantEvaluator::Conditional() {
  CppToken::int_value v1 = Expression(Primary(), 0);
  while (iter_ != tokens_.end()) {
    if (iter_->IsPuncChar('?')) {
      ++iter_;
      CppToken::int_value v2 = Conditional();
      if (iter_ == tokens_.end() || !iter_->IsPuncChar(':')) {
        parser_->Error("syntax error: missing ':' in ternary operation");
        CppToken::int_value r;
        return r;
      }
      ++iter_;
      CppToken::int_value v3 = Conditional();
      CppToken::int_value r;
      r.value = v1.value ? v2.value : v3.value;
      return r;
    }
    break;
  }
  return v1;
}

CppToken::int_value CppIntegerConstantEvaluator::Expression(
    CppToken::int_value v1,
    int min_precedence) {
  while (iter_ != tokens_.end() && iter_->IsOperator() &&
         iter_->GetPrecedence() >= min_precedence) {
    const CppToken& op = *iter_++;
    CppToken::int_value v2 = Primary();
    while (iter_ != tokens_.end() && iter_->IsOperator() &&
           iter_->GetPrecedence() > op.GetPrecedence()) {
      v2 = Expression(v2, iter_->GetPrecedence());
    }
    v1 = op.ApplyOperator(v1, v2);
  }
  return v1;
}

CppToken::int_value CppIntegerConstantEvaluator::Primary() {
  CppToken::int_value result;
  int sign = 1;
  while (iter_ != tokens_.end()) {
    const CppToken& token = *iter_++;
    switch (token.type) {
      case CppToken::IDENTIFIER:
        // If it comes to here without expanded to number, it means
        // identifier is not defined.  Such case should be 0 unless
        // it is the C++ reserved keyword "true".
        if (parser_->is_cplusplus() && token.string_value == "true") {
          // Int value of C++ reserved keyword "true" is 1.
          // See: ISO/IEC 14882:2011 (C++11) 4.5 Integral promotions.
          result.value = 1;
        }
        break;
      case CppToken::UNSIGNED_NUMBER:
        result.unsigned_ = true;
        ABSL_FALLTHROUGH_INTENDED;
      case CppToken::NUMBER:
      case CppToken::CHAR_LITERAL:
        result.value = token.v.int_value;
        break;
      case CppToken::SUB:
        sign = 0 - sign;
        continue;
      case CppToken::ADD:
        continue;
      case CppToken::PUNCTUATOR:
        switch (token.v.char_value.c) {
          case '(':
            result.value = GetValue();
            if (iter_ != tokens_.end() && iter_->IsPuncChar(')')) {
              ++iter_;
            }
            break;
          case '!':
            result.value = !(Primary().value);
            return result;
          case '~':
            result.value = ~(Primary().value);
            return result;
          default: {
            parser_->Error("unknown unary operator: ", token.DebugString());
            break;
          }
        }
        break;
      default:
        break;
    }
    break;
  }
  result.value = sign * result.value;
  return result;
}

}  // namespace devtools_goma
