// Copyright 2017 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_CPP_INPUT_H_
#define DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_CPP_INPUT_H_

#include <memory>
#include <string>

#include "content.h"
#include "cpp_input_stream.h"
#include "include_item.h"

namespace devtools_goma {

class CppInput {
 public:
  CppInput(const CppDirectiveList* directives,
           const std::string& include_guard_ident,
           const std::string& filepath,
           const std::string& directory,
           int include_dir_index,
           size_t line_in_file)
      : filepath_(filepath),
        directory_(directory),
        include_dir_index_(include_dir_index),
        directive_pos_(0),
        line_in_file_(line_in_file),
        directives_(directives),
        include_guard_ident_(include_guard_ident) {}

  const std::string& filepath() const { return filepath_; }
  const std::string& directory() const { return directory_; }
  int include_dir_index() const { return include_dir_index_; }

  size_t directive_pos() const { return directive_pos_; }
  size_t line_in_file() const { return line_in_file_; }

  const std::string& include_guard_ident() const {
    return include_guard_ident_;
  }

  const CppDirective* CurrentDirective() const {
    const CppDirectiveList& directives = *directives_;
    auto current = directive_pos_ - 1;
    if ((current > 0) && (current < directives.size())) {
      return directives[current].get();
    } else {
      return nullptr;
    }
  }

  const CppDirective* NextDirective() {
    const CppDirectiveList& directives = *directives_;
    if (directive_pos_ < directives.size()) {
      return directives[directive_pos_++].get();
    }

    return nullptr;
  }

 private:
  const std::string filepath_;
  const std::string directory_;
  const int include_dir_index_;

  size_t directive_pos_;
  size_t line_in_file_ = 0;
  const CppDirectiveList* directives_;
  const std::string include_guard_ident_;

  DISALLOW_COPY_AND_ASSIGN(CppInput);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_CPP_INPUT_H_
