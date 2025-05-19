// Copyright 2013 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "directive_filter.h"

#include <memory>
#include <string>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "absl/strings/str_cat.h"
#include "content.h"

namespace devtools_goma {

class DirectiveFilterTest : public testing::Test {
};

TEST_F(DirectiveFilterTest, CaptureRawStringLiteral) {
  const std::string kRawString1 = "R\"(\n#include <stdio.h>\n)\"";
  const std::string kRawString2 = "R\"(\nstdio.h)\"";
  const std::string kRawString3 =
      "R\"cpp(\n  class $Class[[A]] {\n   "
      "  #include \"imp.h\"\n"
      " };\n"
      ")cpp\"";
  const std::string kRawString4 =
      "R\"(#version 310 es\n"
      "  #extension GL_EXT_geometry_shader : require\n"
      "  layout (points) in;\n"
      "  layout (invocations = 2, local_size_x = 15) in;\n"
      "  layout (points, max_vertices = 2) out;\n"
      "  void main()\n"
      "  {\n"
      "  })\"";
  std::string src = absl::StrCat(kRawString1, "\n", kRawString2, "\n",
                                 kRawString3, "\n", kRawString4, "\n");
  const char* pos = src.c_str();
  const char* end = src.c_str() + src.size();

  int num = DirectiveFilter::CaptureRawStringLiteral(pos, end);
  EXPECT_EQ(kRawString1, absl::string_view(pos, num));
  pos += num;
  EXPECT_NE(pos, end);
  EXPECT_EQ('\n', *pos);
  pos++;
  num = DirectiveFilter::CaptureRawStringLiteral(pos, end);
  EXPECT_EQ(kRawString2, absl::string_view(pos, num));
  pos += num;
  EXPECT_NE(pos, end);
  EXPECT_EQ('\n', *pos);
  pos++;
  num = DirectiveFilter::CaptureRawStringLiteral(pos, end);
  EXPECT_EQ(kRawString3, absl::string_view(pos, num));
  pos += num;
  EXPECT_NE(pos, end);
  EXPECT_EQ('\n', *pos);
  pos++;
  num = DirectiveFilter::CaptureRawStringLiteral(pos, end);
  EXPECT_EQ(kRawString4, absl::string_view(pos, num));
  pos += num;
  EXPECT_NE(pos, end);
  EXPECT_EQ('\n', *pos);
}

TEST_F(DirectiveFilterTest, SkipSpaces) {
  std::string src = "    12   3 \\\n 4 \\\n\\\n   5  \\\r\n  6  \\\n";
  const char* pos = src.c_str();
  const char* end = src.c_str() + src.size();

  pos = DirectiveFilter::SkipSpaces(pos, end);
  EXPECT_EQ('1', *pos);

  ++pos;
  pos = DirectiveFilter::SkipSpaces(pos, end);
  EXPECT_EQ('2', *pos);

  ++pos;
  pos = DirectiveFilter::SkipSpaces(pos, end);
  EXPECT_EQ('3', *pos);

  ++pos;
  pos = DirectiveFilter::SkipSpaces(pos, end);
  EXPECT_EQ('4', *pos);

  ++pos;
  pos = DirectiveFilter::SkipSpaces(pos, end);
  EXPECT_EQ('5', *pos);

  ++pos;
  pos = DirectiveFilter::SkipSpaces(pos, end);
  EXPECT_EQ('6', *pos);

  ++pos;
  pos = DirectiveFilter::SkipSpaces(pos, end);
  EXPECT_EQ(end, pos);
}

TEST_F(DirectiveFilterTest, NextLineHead) {
  std::string src = "\n1    \\\n  \n2  \\\n\\\n\\\r\n\n3   \\\r\n";
  const char* pos = src.c_str();
  const char* end = src.c_str() + src.size();

  pos = DirectiveFilter::NextLineHead(pos, end);
  EXPECT_EQ('1', *pos);

  ++pos;
  pos = DirectiveFilter::NextLineHead(pos, end);
  EXPECT_EQ('2', *pos);

  ++pos;
  pos = DirectiveFilter::NextLineHead(pos, end);
  EXPECT_EQ('3', *pos);

  ++pos;
  pos = DirectiveFilter::NextLineHead(pos, end);
  EXPECT_EQ(end, pos);
}

TEST_F(DirectiveFilterTest, RemovesBlockComment) {
  // All comments will be removed.
  std::string src = "/* foo bar */";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  EXPECT_EQ(
      "", std::string(filtered->buf(), filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, RemoveNonComment) {
  // All comments will be removed.
  std::string src = "foo bar";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  EXPECT_EQ(
      "", std::string(filtered->buf(), filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, RemovesBlockCommentContainingOnelineComment) {
  std::string src = "/* // */";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  EXPECT_EQ(
      "", std::string(filtered->buf(), filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, RemovesOnelineComment) {
  std::string src = "// foo bar";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  EXPECT_EQ(
      "", std::string(filtered->buf(), filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, RemovesOnelineCommentContainingBlockCommentStart1) {
  std::string src = "// /*";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  EXPECT_EQ(
      "", std::string(filtered->buf(), filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, RemovesOnelineCommentContainingBlockCommentStart2) {
  std::string src = "// /*\n*/";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  EXPECT_EQ(
      "\n", std::string(filtered->buf(), filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, RemovesComplexBlockComment) {
  std::string src = "/*/ #include <iostream> /*/";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  EXPECT_EQ(
      "", std::string(filtered->buf(), filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, BlockCommentIsNotFinished) {
  std::string src = "/* #include <iostream>";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  EXPECT_EQ(
      "", std::string(filtered->buf(), filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, RemoveRawStringLiteralWithDirective) {
  const std::string kRawString1 = "R\"(\n#include <stdio.h>\n)\"";
  const std::string kRawString2 = "R\"(stdio.h)\"";
  const std::string kRawString3 =
      "R\"cpp(\n  class $Class[[A]] {\n   "
      "  #include \"imp.h\"\n"
      " };\n"
      ")cpp\"";
  const std::string src =
      absl::StrCat(kRawString1, "\n", kRawString2, "\n", kRawString3, "\n");

  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  EXPECT_EQ("\n\n\n\n\n\n\n\n\n", absl::string_view(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, FilterDirectives) {
  std::string src =
      "#include <iostream>\n"
      " f(); g(); h(); \n"
      "#include <iomanip>\n";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected =
      "#include <iostream>\n"
      "\n"
      "#include <iomanip>\n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, DirectiveIsDividedWithBackslashAndLF) {
  std::string src =
      "#include \\\n"
      "<iostream>";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "#include <iostream>\n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, DirectiveIsDividedWithBackslashAndLFLF) {
  std::string src =
      "#include \\\n\\\n"
      "<iostream>";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "#include <iostream>\n\n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, DirectiveIsDividedWithBackslashAndCRLF) {
  std::string src =
      "#include \\\r\n"
      "<iostream>";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "#include <iostream>\n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, EmptyLineAndBackslashLFBeforeDirective) {
  std::string src =
      "                \\\n"
      "#include <iostream>";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  EXPECT_EQ(
      "\n#include <iostream>",
      std::string(filtered->buf(), filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, EmptyLineAndBackslashLFLFBeforeDirective) {
  std::string src =
      "                \\\n\\\n"
      "#include <iostream>";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  EXPECT_EQ(
      "\n\n#include <iostream>",
      std::string(filtered->buf(), filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, EmptyLineAndBackslashCRLFBeforeDirective) {
  std::string src =
      "                \\\r\n"
      "#include <iostream>";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  EXPECT_EQ(
      "\n#include <iostream>",
      std::string(filtered->buf(), filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, DirectiveIsDividedWithComments) {
  std::string src =
      "#include /*\n"
      " something */\\\n"
      "<iostream>\n";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  EXPECT_EQ(
      "#include  <iostream>\n\n\n",
      std::string(filtered->buf(), filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, FilterDirectivesWithContinuingLines4) {
  std::string src =
      "      #include <iostream>\n"
      "  #endif\n"
      " #include /* hoge */\n";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected =
      "#include <iostream>\n"
      "#endif\n"
      "#include  \n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, DirectiveContainsComments) {
  std::string src =
      "      #include <iostream>  //\n"
      "  #endif /* \n"
      " #include /* hoge */\n";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected =
      "#include <iostream>  \n"
      "#endif  \n"
      "\n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, OneLineCommentContainsBlockComment) {
  std::string src = "// /* \n#include <iostream>\n";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "\n#include <iostream>\n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, IncludePathContainsSlashSlash) {
  std::string src = "#include \"foo//bar\"\n";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "#include \"foo//bar\"\n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

// When just keeping #include containing //, we might miss other comment start.
TEST_F(DirectiveFilterTest, IncludePathContainsSlashSlash2) {
  std::string src = "#include \"foo//bar\" /*\n hoge */\n";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "#include \"foo//bar\"  \n\n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

// When just keeping #include containing //, we might miss other comment start.
TEST_F(DirectiveFilterTest, IncludePathContainsSlashSlash3) {
  std::string src = "#include \"foo//bar\" // hoge */\n";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "#include \"foo//bar\" \n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, StrayDoubleQuotation) {
  std::string src = "\"\n#include <iostream>\n";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "\n#include <iostream>\n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, StrayDoubleQuotation2) {
  std::string src = "#include <iostream> \"\n";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "#include <iostream> \"\n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, BlockCommentStartInString) {
  std::string src = "\"ho/*ge\"\n#include <iostream>\n\"fu*/ga\"";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "\n#include <iostream>\n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, LineCommentStartInString) {
  std::string src = "#define HOGE \"HOGE\\FUGA\"\n";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "#define HOGE \"HOGE\\FUGA\"\n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, MultipleLineString) {
  std::string src =
      "#define HOGE \"HOGE\\\n"
      "//\\\"hoge\\\"\\\n"
      "FUGA\"\n";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "#define HOGE \"HOGE//\\\"hoge\\\"FUGA\"\n\n\n";
  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, StringContainingDoubleQuotation) {
  std::string src =
      "#define HOGE \"HOGE\\\"\\\n"
      "//\\\"hoge\\\"\\\n"
      "FUGA\"\n";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "#define HOGE \"HOGE\\\"//\\\"hoge\\\"FUGA\"\n\n\n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, MultipleLineDirectiveAndIdentifier) {
  std::string src =
      "#de\\\n"
      "fi\\\n"
      "ne\\\n"
      " \\\n"
      "H\\\n"
      "OG\\\n"
      "E";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "#define HOGE\n\n\n\n\n\n";

  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, SkipRawStringLiteral) {
  // b/180559934
  std::string src =
      "checkHighlightings(R\"cpp(\n"
      "  class $Class[[A]] {\n"
      "   #include \"imp.h\n"
      "  };\n"
      ")cpp\",";
  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  std::string expected = "\n\n\n\n";
  EXPECT_EQ(expected, std::string(filtered->buf(),
                                  filtered->buf_end() - filtered->buf()));
}

TEST_F(DirectiveFilterTest, HandleConcatOfStringAndValueEndingInR) {
  const std::string src =
      "#define BAR \"bar\" \n"
      "#define BAZ \\\n"
      "    BAR\"baz\" \n"
      "#define FOO(x) x*x\n"
      "#define OK \n";

  const std::string expected =
      "#define BAR \"bar\" \n"
      "#define BAZ     BAR\"baz\" \n"
      "\n"
      "#define FOO(x) x*x\n"
      "#define OK \n";

  std::unique_ptr<Content> content(Content::CreateFromString(src));
  std::unique_ptr<Content> filtered(
      DirectiveFilter::MakeFilteredContent(*content));

  EXPECT_EQ(expected, absl::string_view(filtered->buf(),
                                        filtered->buf_end() - filtered->buf()));
}

}  // namespace devtools_goma
