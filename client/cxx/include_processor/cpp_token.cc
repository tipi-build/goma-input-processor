// Copyright 2017 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cpp_token.h"

namespace {

typedef devtools_goma::CppToken::int_value int_value;

int_value Mul(int_value v1, int_value v2) {
  int_value r;
  r.unsigned_ = v1.unsigned_ | v2.unsigned_;
  if (r.unsigned_) {
    r.value = static_cast<uint64_t>(v1.value) * static_cast<uint64_t>(v2.value);
  } else {
    r.value = v1.value * v2.value;
  }
  return r;
}

int_value Div(int_value v1, int_value v2) {
  int_value r;
  r.unsigned_ = v1.unsigned_ | v2.unsigned_;
  if (v2.value == 0) {
    r.value = 0;
    return r;
  }
  if (r.unsigned_) {
    r.value = static_cast<uint64_t>(v1.value) / static_cast<uint64_t>(v2.value);
  } else {
    r.value = v1.value / v2.value;
  }
  return r;
}

int_value Mod(int_value v1, int_value v2) {
  int_value r;
  r.unsigned_ = v1.unsigned_ | v2.unsigned_;
  if (v2.value == 0) {
    r.value = 0;
    return r;
  }
  if (r.unsigned_) {
    r.value = static_cast<uint64_t>(v1.value) / static_cast<uint64_t>(v2.value);
  } else {
    r.value = v1.value % v2.value;
  }
  return r;
}

int_value Add(int_value v1, int_value v2) {
  int_value r;
  r.unsigned_ = v1.unsigned_ | v2.unsigned_;
  r.value = v1.value + v2.value;
  return r;
}

int_value Sub(int_value v1, int_value v2) {
  int_value r;
  r.unsigned_ = v1.unsigned_ | v2.unsigned_;
  r.value = v1.value - v2.value;
  return r;
}

int_value RShift(int_value v1, int_value v2) {
  int_value r;
  r.unsigned_ = v1.unsigned_ | v2.unsigned_;
  r.value = v1.value >> v2.value;
  return r;
}

int_value LShift(int_value v1, int_value v2) {
  int_value r;
  r.unsigned_ = v1.unsigned_ | v2.unsigned_;
  r.value = v1.value << v2.value;
  return r;
}

int_value Gt(int_value v1, int_value v2) {
  int_value r;
  if (v1.unsigned_ | v2.unsigned_) {
    r.value = static_cast<uint64_t>(v1.value) > static_cast<uint64_t>(v2.value);
  } else {
    r.value = v1.value > v2.value;
  }
  return r;
}

int_value Lt(int_value v1, int_value v2) {
  int_value r;
  if (v1.unsigned_ | v2.unsigned_) {
    r.value = static_cast<uint64_t>(v1.value) < static_cast<uint64_t>(v2.value);
  } else {
    r.value = v1.value < v2.value;
  }
  return r;
}

int_value Ge(int_value v1, int_value v2) {
  int_value r;
  if (v1.unsigned_ | v2.unsigned_) {
    r.value =
        static_cast<uint64_t>(v1.value) >= static_cast<uint64_t>(v2.value);
  } else {
    r.value = v1.value >= v2.value;
  }
  return r;
}

int_value Le(int_value v1, int_value v2) {
  int_value r;
  if (v1.unsigned_ | v2.unsigned_) {
    r.value =
        static_cast<uint64_t>(v1.value) <= static_cast<uint64_t>(v2.value);
  } else {
    r.value = v1.value <= v2.value;
  }
  return r;
}

int_value Eq(int_value v1, int_value v2) {
  int_value r;
  r.value = v1.value == v2.value;
  return r;
}

int_value Ne(int_value v1, int_value v2) {
  int_value r;
  r.value = v1.value != v2.value;
  return r;
}

int_value And(int_value v1, int_value v2) {
  int_value r;
  r.unsigned_ = v1.unsigned_ | v2.unsigned_;
  if (r.unsigned_) {
    r.value = static_cast<uint64_t>(v1.value) & static_cast<uint64_t>(v2.value);
  } else {
    r.value = v1.value & v2.value;
  }
  return r;
}

int_value Xor(int_value v1, int_value v2) {
  int_value r;
  r.unsigned_ = v1.unsigned_ | v2.unsigned_;
  if (r.unsigned_) {
    r.value = static_cast<uint64_t>(v1.value) ^ static_cast<uint64_t>(v2.value);
  } else {
    r.value = v1.value ^ v2.value;
  }
  return r;
}

int_value Or(int_value v1, int_value v2) {
  int_value r;
  r.unsigned_ = v1.unsigned_ | v2.unsigned_;
  if (r.unsigned_) {
    r.value = static_cast<uint64_t>(v1.value) | static_cast<uint64_t>(v2.value);
  } else {
    r.value = v1.value | v2.value;
  }
  return r;
}

int_value LAnd(int_value v1, int_value v2) {
  int_value r;
  r.value = v1.value && v2.value;
  return r;
}

int_value LOr(int_value v1, int_value v2) {
  int_value r;
  r.value = v1.value || v2.value;
  return r;
}

}  // anonymous namespace

namespace devtools_goma {

const int CppToken::kPrecedenceTable[] = {
  9, 9, 9,      // MUL, DIV, MOD,
  8, 8,         // ADD, SUB,
  7, 7,         // RSHIFT, LSHIFT,
  6, 6, 6, 6,   // GT, LT, GE, LE,
  5, 5,         // EQ, NE,
  4,            // AND,
  3,            // XOR,
  2,            // OR,
  1,            // LAND,
  0,            // LOR,
};

const CppToken::OperatorFunction CppToken::kFunctionTable[] = {
  Mul, Div, Mod, Add, Sub, RShift, LShift, Gt, Lt, Ge, Le, Eq, Ne,
  And, Xor, Or, LAnd, LOr
};

std::string CppToken::DebugString() const {
  std::string str;
  str.reserve(16);
  switch (type) {
    case IDENTIFIER:
      str.append("[IDENT(");
      str.append(string_value);
      str.append(")]");
      break;
    case STRING:
      str.append("[STRING(\"");
      str.append(string_value);
      str.append("\")]");
      break;
    case NUMBER:
      str.append("[NUMBER(");
      str.append(string_value);
      str.append(", ");
      str.append(std::to_string(v.int_value));
      str.append(")]");
      break;
    case UNSIGNED_NUMBER:
      str.append("[UNSIGNED_NUMBER(");
      str.append(string_value);
      str.append(", ");
      str.append(std::to_string(v.int_value));
      str.append(")]");
      break;
    case DOUBLESHARP:
      return "[##]";
    case TRIPLEDOT:
      return "[...]";
    case NEWLINE:
      return "[NL]\n";
    case ESCAPED:
      str.append("[\\");
      str.push_back(v.char_value.c);
      str.append("]");
      break;
    case MACRO_PARAM:
      str.append("[MACRO_PARAM(arg");
      str.append(std::to_string(v.param_index));
      str.append(")]");
      break;
    case MACRO_PARAM_VA_ARGS:
      str.append("[MACRO_PARAM_VA_ARGS]");
      break;
    case CHAR_LITERAL:
      str.append("[CHAR_LITERAL(");
      str.append(std::to_string(v.int_value));
      str.append(")]");
      break;
    case END:
      return "[END]";
    default:
      str.append("[");
      if (!string_value.empty()) {
        str.append(string_value);
      } else if (v.char_value.c) {
        str.push_back(v.char_value.c);
      } else {
        str.append(v.char_value.c2);
      }
      str.append("]");
  }
  return str;
}

std::string CppToken::GetCanonicalString() const {
  if (!string_value.empty())
    return string_value;
  if (v.char_value.c)
    return std::string() + v.char_value.c;
  return v.char_value.c2;
}

#ifdef MEMORY_SANITIZER
bool CppToken::IsPuncChar(int c) const {
  return ((type == PUNCTUATOR || type >= OP_BEGIN) && v.int_value == c);
}
#endif  // MEMORY_SANITIZER

}  // namespace devtools_goma
