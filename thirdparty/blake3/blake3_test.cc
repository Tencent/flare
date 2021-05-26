// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
//
// Author: Gao Lu <luobogao@tencent.com>

#include "thirdparty/blake3/blake3.h"

#include <string>

#include "gtest/gtest.h"

char buffer[1048576 * 1024];  // 1G

// Adapted from example at https://github.com/BLAKE3-team/BLAKE3/tree/master/c
std::string GetHash(const char* ptr, size_t length) {
  blake3_hasher hasher;
  blake3_hasher_init(&hasher);

  blake3_hasher_update(&hasher, ptr, length);

  uint8_t output[BLAKE3_OUT_LEN];
  blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);

  std::string result;
  for (size_t i = 0; i < BLAKE3_OUT_LEN; i++) {
    char buf[10];
    sprintf(buf, "%02x", output[i]);
    result += buf;
  }
  return result;
}

TEST(Blake3, All) {
  EXPECT_EQ("94b4ec39d8d42ebda685fbb5429e8ab0086e65245e750142c1eea36a26abc24d",
            GetHash(buffer, sizeof(buffer)));
}
