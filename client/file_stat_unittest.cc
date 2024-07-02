// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "file_stat.h"

#include <gtest/gtest.h>
#include <cstdio>

#include "absl/hash/hash_testing.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "file_helper.h"
#include "glog/logging.h"
#include "path.h"
#include "scoped_tmp_file.h"

namespace devtools_goma {

namespace {

// The file timestamp might only have 1-second resolution, and might not be
// completely in sync with the source of absl::Now(). To avoid any flaky tests,
// allow for this much margin of error when comparing file timestamps against
// absl::Now(). The goal is to make sure that we are getting a file timestamp
// that is recent, but not necessarily down to sub-second precision.
constexpr absl::Duration kFileStatMtimeMarginOfError = absl::Seconds(2);

}  // namespace

TEST(FileStatTest, DefaultConstructor) {
  FileStat dummy_stat;

  EXPECT_FALSE(dummy_stat.IsValid());
  EXPECT_FALSE(dummy_stat.mtime.has_value());
}

TEST(FileStatTest, InitFromDirectory) {
  const absl::Time start_time = absl::Now();
  ScopedTmpDir dir("dir");

  FileStat dir_stat(dir.dirname());

  EXPECT_TRUE(dir_stat.IsValid()) << dir_stat;
  EXPECT_TRUE(dir_stat.is_directory) << dir_stat;

  ASSERT_TRUE(dir_stat.mtime.has_value()) << dir_stat;
  EXPECT_GE(*dir_stat.mtime, start_time - kFileStatMtimeMarginOfError)
      << dir_stat;
}

TEST(FileStatTest, InitFromEmptyFile) {
  const absl::Time start_time = absl::Now();
  ScopedTmpFile file("file");

  FileStat file_stat(file.filename());

  EXPECT_TRUE(file_stat.IsValid()) << file_stat;
  EXPECT_EQ(0, file_stat.size) << file_stat;
  EXPECT_FALSE(file_stat.is_directory) << file_stat;

  EXPECT_TRUE(file_stat.mtime.has_value()) << file_stat;
  EXPECT_GE(*file_stat.mtime, start_time - kFileStatMtimeMarginOfError)
      << file_stat;
}

TEST(FileStatTest, InitFromNonEmptyFile) {
  const absl::Time start_time = absl::Now();
  const std::string kContents = "The quick brown fox jumps over the lazy dog.";
  ScopedTmpFile file("file");
  file.Write(kContents.c_str(), kContents.size());

  FileStat file_stat(file.filename());

  EXPECT_TRUE(file_stat.IsValid()) << file_stat;
  EXPECT_EQ(kContents.size(), file_stat.size) << file_stat;
  EXPECT_FALSE(file_stat.is_directory) << file_stat;

  EXPECT_TRUE(file_stat.mtime.has_value()) << file_stat;
  EXPECT_GE(*file_stat.mtime, start_time - kFileStatMtimeMarginOfError)
      << file_stat;
}

TEST(FileStatTest, ValidVersusInvalid) {
  ScopedTmpFile file("file");

  FileStat valid(file.filename());
  FileStat invalid;

  EXPECT_NE(valid, invalid);
}

TEST(FileStatTest, SameFile) {
  ScopedTmpFile file("file");

  FileStat file_stat1(file.filename());
  FileStat file_stat2(file.filename());

  EXPECT_EQ(file_stat1, file_stat2);
}

TEST(FileStatTest, DifferentTime) {
  // Instead of trying to create different files, manually fill these out.
  FileStat stat1, stat2, stat3;
  FileStat stat_notime1, stat_notime2;

  // The first three have valid timestamps.
  stat1.mtime = absl::FromTimeT(100);
  stat1.size = 0;

  stat2.mtime = absl::FromTimeT(200);
  stat2.size = 0;

  stat3.mtime = absl::FromTimeT(200);
  stat3.size = 0;

  // These do not have valid timestamps -- but fill in the timestamp value
  // before clearing it.
  stat_notime1.mtime = absl::FromTimeT(100);
  stat_notime1.mtime.reset();
  stat_notime1.size = 0;

  stat_notime2.mtime = absl::FromTimeT(200);
  stat_notime2.mtime.reset();
  stat_notime2.size = 0;

  EXPECT_NE(stat1, stat2);  // Different valid time values.
  EXPECT_EQ(stat2, stat3);  // Same valid time values.

  EXPECT_EQ(stat_notime1, stat_notime2);  // No time values set: should be same.

  // Empty time values should not match valid time values.
  EXPECT_NE(stat1, stat_notime1);
  EXPECT_NE(stat2, stat_notime2);
}

TEST(FileStatTest, Symlink) {
  ScopedTmpDir dir("file_stat_symlink");
  ASSERT_TRUE(dir.valid());
  const absl::string_view data = "some data";
  const std::string target_name = "file";
  const std::string filename = file::JoinPath(dir.dirname(), target_name);
  ASSERT_TRUE(WriteStringToFile(data, filename));
  const std::string symlink_name =
      file::JoinPath(dir.dirname(), "symlink_to_file");
#ifndef _WIN32
  int r = symlink(target_name.c_str(), symlink_name.c_str());
  if (r != 0) {
    PLOG(ERROR) << "Failed to symlink " << filename << " <- " << symlink_name;
    ASSERT_EQ(r, 0);
  }
#else
  DWORD r = CreateSymbolicLinkA(symlink_name.c_str(), target_name.c_str(), 0);
  if (r == 0) {
    LOG_SYSRESULT(GetLastError());
    LOG(ERROR) << "Failed to symlink " << filename << " <- " << symlink_name;
    // TODO: enable this test again.
    LOG(ERROR) << "Skip this test.";
    return;
  }
#endif
  FileStat file_stat(filename);
  EXPECT_TRUE(file_stat.IsValid()) << file_stat;
  FileStat symlink_stat(symlink_name);
  EXPECT_TRUE(symlink_stat.IsValid()) << symlink_stat;
  EXPECT_EQ(file_stat, symlink_stat);
  EXPECT_EQ(symlink_stat.size, data.size());
}

TEST(FileStatTest, Hash) {
  FileStat stat1, stat2, stat3, stat4;
  stat1.is_directory = true;
  stat2.size = 1;
  stat3.mtime = absl::FromTimeT(100);

  stat4.is_directory = true;
  stat4.taken_at = stat1.taken_at + absl::Seconds(1);

  EXPECT_EQ(stat1, stat4);
  EXPECT_NE(stat1.taken_at, stat4.taken_at);

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      stat1,
      stat2,
      stat3,
      stat4,
  }));
}

}  // namespace devtools_goma
