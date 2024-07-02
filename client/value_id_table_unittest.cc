// Copyright 2022 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "value_id_table.h"

#include <gtest/gtest.h>

namespace devtools_goma {

TEST(ValueIDTableTest, Basic) {
  ValueIDTable<int> table;

  auto id1 = table.GetId(5);
  EXPECT_EQ(id1, 0);
  EXPECT_EQ(table.GetId(5), 0);

  auto id2 = table.GetId(8);
  EXPECT_EQ(id2, 1);

  EXPECT_EQ(table.GetValue(id1), 5);
  EXPECT_EQ(table.GetValue(id2), 8);

  table.Clear();

  id1 = table.GetId(15);
  EXPECT_EQ(id1, 0);
  EXPECT_EQ(table.GetId(15), 0);

  id2 = table.GetId(18);
  EXPECT_EQ(id2, 1);

  EXPECT_EQ(table.GetValue(id1), 15);
  EXPECT_EQ(table.GetValue(id2), 18);
}

}  // namespace devtools_goma
