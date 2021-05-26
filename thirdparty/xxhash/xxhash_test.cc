// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
//
// Author: Gao Lu <luobogao@tencent.com>

#include "xxhash.h"

#include <string>

#include "gtest/gtest.h"

char buffer[1024 * 1024 * 1024];

TEST(XXH64, All) {
  EXPECT_EQ(14959503861083998079ULL, XXH64(buffer, sizeof(buffer), 0));
}
