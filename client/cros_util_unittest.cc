// Copyright 2014 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "cros_util.h"

#include<string>
#include<vector>

#include <gtest/gtest.h>

namespace devtools_goma {

TEST(CrosUtil, IsDenied) {
  std::vector<std::string> denylist;
  denylist.push_back("/tmp");
  EXPECT_TRUE(IsDenied("/tmp", denylist));
  denylist.clear();

  denylist.push_back("non-related");
  denylist.push_back("/tmp");
  EXPECT_TRUE(IsDenied("/tmp", denylist));
  denylist.clear();

  denylist.push_back("/usr");
  denylist.push_back("/tmp");
  EXPECT_TRUE(IsDenied("/usr/local/etc", denylist));
  denylist.clear();

  denylist.push_back("non-related");
  denylist.push_back("/local");
  EXPECT_TRUE(IsDenied("/usr/local/etc", denylist));
  denylist.clear();

  denylist.push_back("non-related");
  denylist.push_back("/etc");
  EXPECT_TRUE(IsDenied("/usr/local/etc", denylist));
  denylist.clear();

  EXPECT_FALSE(IsDenied("/tmp", denylist));
  denylist.clear();

  denylist.push_back("non-related");
  EXPECT_FALSE(IsDenied("/tmp", denylist));
  denylist.clear();

  denylist.push_back("/opt");
  denylist.push_back("/tmp");
  EXPECT_FALSE(IsDenied("/usr/local/etc", denylist));
  denylist.clear();
}

TEST(CrosUtil, GetLoadAverage) {
  // Smoke test
  EXPECT_GE(GetLoadAverage(), 0.0);
}

TEST(CrosUtil, RandInt64) {
  const int64_t kInt64Offset = static_cast<int64_t>(INT32_MAX) + 1;
  static_assert(kInt64Offset > 0 && kInt64Offset > INT32_MAX,
                "Did not create int64 value correctly");
  // Smoke test
  for (int64_t i = 0; i < 100; ++i) {
    int64_t r = RandInt64(10, 20);
    EXPECT_LT(r, 21);
    EXPECT_GT(r, 9);

    r = RandInt64(10 + kInt64Offset, 20 + kInt64Offset);
    EXPECT_LT(r, 21 + kInt64Offset);
    EXPECT_GT(r, 9 + kInt64Offset);
  }
  EXPECT_EQ(128, RandInt64(128, 128));
  EXPECT_EQ(128 + kInt64Offset,
            RandInt64(128 + kInt64Offset, 128 + kInt64Offset));
}

}  // namespace devtools_goma
