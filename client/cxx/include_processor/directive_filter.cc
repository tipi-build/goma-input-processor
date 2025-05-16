// Copyright 2013 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "directive_filter.h"

#include <string.h>

#include <memory>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "content.h"
#include "glog/logging.h"

namespace devtools_goma {

// static
std::unique_ptr<Content> DirectiveFilter::MakeFilteredContent(
    const Content& content) {
  const size_t content_length = content.size();
  std::unique_ptr<char[]> buffer(new char[content_length + 1]);

  size_t length = RemoveComments(content.buf(), content.buf_end(),
                                 buffer.get());

  length = FilterOnlyDirectives(buffer.get(), buffer.get() + length,
                                buffer.get());

  length = RemoveEscapedNewLine(buffer.get(), buffer.get() + length,
                                buffer.get());

  return Content::CreateFromBuffer(buffer.get(), length);
}

// static
const char* DirectiveFilter::SkipSpaces(const char* pos, const char* end) {
  while (pos != end) {
    if (*pos == ' ' || *pos == '\t') {
      ++pos;
      continue;
    }

    int newline_byte = IsEscapedNewLine(pos, end);
    if (newline_byte > 0) {
      pos += newline_byte;
      continue;
    }

    return pos;
  }

  return end;
}

/* static */
const char* DirectiveFilter::NextLineHead(const char* pos, const char* end) {
  while (pos != end) {
    if (*pos == '\n')
      return pos + 1;

    int newline_byte = IsEscapedNewLine(pos, end);
    if (newline_byte)
      pos += newline_byte;
    else
      pos += 1;
  }

  return end;
}

// static
int DirectiveFilter::CaptureRawStringLiteral(const char* pos, const char* end) {
  absl::string_view input(pos, end - pos);
  absl::string_view s = input;
  CHECK(absl::ConsumePrefix(&s, "R\"")) << input;
  absl::string_view::size_type p = s.find('(');
  if (p == absl::string_view::npos) {
    s = input;
    if (s.size() >= 80UL) {
      s = s.substr(0, 80);
    }
    LOG(ERROR) << "failed to detect raw string literal:" << s;
    return input.size();
  }
  absl::string_view delimiter = s.substr(0, p);
  CHECK_EQ(s[p], '(');
  s = s.substr(p + 1);
  int n = 0;
  while (!s.empty()) {
    p = s.find(')');
    if (p == absl::string_view::npos) {
      return input.size();
    }
    CHECK_EQ(s[p], ')');
    absl::string_view r = s.substr(p + 1);
    n += p;
    if (absl::ConsumePrefix(&r, delimiter) && !r.empty() && r[0] == '"') {
      // raw string literal ends.
      return strlen("R\"") + delimiter.size() + strlen("(") + n + strlen(")") +
             delimiter.size() + strlen("\"");
    }
    n++;
    s = r;
  }
  return input.size();
}

// static
int DirectiveFilter::CopyStringLiteral(const char* pos, const char* end,
                                       char* dst) {
  const char* initial_pos = pos;

  DCHECK_EQ(*pos, '\"');
  DCHECK(pos != end);

  // Copy '\"'
  *dst++ = *pos++;

  while (pos != end) {
    // String literal ends.
    if (*pos == '\"') {
      *dst++ = *pos++;
      break;
    }

    // Corresponding " was not found. Keep this as is.
    if (*pos == '\n') {
      *dst++ = *pos++;
      break;
    }

    int newline_byte = IsEscapedNewLine(pos, end);
    if (newline_byte > 0) {
      while (newline_byte--) {
        *dst++ = *pos++;
      }
      continue;
    }

    // \" does not end string literal.
    // I don't think we need to support trigraph. So, we don't consider "??/",
    // which means "\".
    if (*pos == '\\' && pos + 1 != end && *(pos + 1) == '\"') {
      *dst++ = *pos++;
      *dst++ = *pos++;
      continue;
    }

    *dst++ = *pos++;
  }

  return pos - initial_pos;
}

// static
int DirectiveFilter::IsEscapedNewLine(const char* pos, const char* end) {
  if (*pos != '\\')
    return 0;

  if (pos + 1 < end && *(pos + 1) == '\n')
    return 2;

  if (pos + 2 < end && *(pos + 1) == '\r' && *(pos + 2) == '\n')
    return 3;

  return 0;
}

// Copied |src| to |dst| with removing comments.
// It also removes raw string literal that contains "#", because
// such string literal confuses include processor (detect it as
// directives) and assumes such literal may not be used for
// C preprocessor. http://b/123493120
// TODO: We assume '"' is not in include pathname.
// When such pathname exists, this won't work well. e.g. #include <foo"bar>
// static
size_t DirectiveFilter::RemoveComments(const char* src, const char* end,
                                       char* dst) {
  const char* original_src = src;
  const char* original_dst = dst;

  while (src != end) {
    // Raw string literal starts.
    if (*src == 'R' && src + 1 < end && *(src + 1) == '\"' &&
        (src == original_src ||
         (!absl::ascii_isalnum(*(src - 1)) && *(src - 1) != '_'))) {
      int num = CaptureRawStringLiteral(src, end);
      const absl::string_view literal(src, num);
      VLOG(5) << "raw string literal=" << literal;
      if (literal.find('#') != absl::string_view::npos) {
        src += num;
        continue;
      }
      // copy it as is.
      memcpy(dst, src, num);
      src += num;
      dst += num;
      continue;
    }
    // String starts.
    if (*src == '\"') {
      int num_copied = CopyStringLiteral(src, end, dst);
      src += num_copied;
      dst += num_copied;
      continue;
    }

    // Check a comment does not start.
    if (*src != '/' || src + 1 == end) {
      *dst++ = *src++;
      continue;
    }

    // Block comment starts.
    if (*(src + 1) == '*') {
      const char* end_comment = nullptr;
      const char* pos = src + 2;
      size_t newlines_in_comment = 0;
      while (pos + 2 <= end) {
        if (*pos == '\n') { ++newlines_in_comment; }
        if (*pos == '*' && *(pos + 1) == '/') {
          end_comment = pos;
          break;
        }
        ++pos;
      }

      // When block comment end is not found, we don't skip them.
      if (end_comment == nullptr) {
        while (src < end)
          *dst++ = *src++;
        return dst - original_dst;
      }


      src = end_comment + 2;
      *dst++ = ' ';

      for (size_t newlines = 0; newlines < newlines_in_comment; newlines++) {
       *dst++ = '\n';
      }
      continue;
    }

    // One-line comment starts.
    if (*(src + 1) == '/') {
      src = DirectiveFilter::NextLineHead(src + 2, end);
      *dst++ = '\n';
      continue;
    }

    *dst++ = *src++;
  }

  return dst - original_dst;
}

// static
size_t DirectiveFilter::RemoveEscapedNewLine(
    const char* src, const char* end, char* dst) {
  const char* initial_dst = dst;

  while (src != end) {
    int newline_bytes = IsEscapedNewLine(src, end);
    if (newline_bytes == 0) {
      *dst++ = *src++;
    } else {
      src += newline_bytes;
    }
  }

  return dst - initial_dst;
}

// static
size_t DirectiveFilter::FilterOnlyDirectives(
    const char* src, const char* end, char* dst) {
  const char* const original_dst = dst;

  while (src != end) {
    src = DirectiveFilter::SkipSpaces(src, end);

    if (src != end && *src == '#') {
      *dst++ = *src++;
      // Omit spaces after '#' in directive.
      src = DirectiveFilter::SkipSpaces(src, end);
      const char* next_line_head = DirectiveFilter::NextLineHead(src, end);
      memmove(dst, src, next_line_head - src);
      dst += next_line_head - src;
      src = next_line_head;
    } else {
      const char* next_line_head = DirectiveFilter::NextLineHead(src, end);
      *dst++ = '\n'; // Keep track of skipped line
      src = next_line_head;
    }
  }

  return dst - original_dst;
}

}  // namespace devtools_goma
