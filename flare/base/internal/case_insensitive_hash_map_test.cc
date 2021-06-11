// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of the
// License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "flare/base/internal/case_insensitive_hash_map.h"

#include <string>

#include "googletest/gtest/gtest.h"

namespace flare::internal {

TEST(CaseInsensitiveHashMap, Easy) {
  CaseInsensitiveHashMap<std::string, std::string> m;

  m["a"] = "1";
  m["b"] = "10";
  m["C"] = "-5";
  m["c"] = "3";  // Overwrites "C"

  ASSERT_EQ("1", *m.TryGet("a"));
  ASSERT_EQ("1", *m.TryGet("A"));
  ASSERT_EQ("3", m["C"]);
  ASSERT_FALSE(m.TryGet("d"));
  ASSERT_EQ(3, m.size());
}

}  // namespace flare::internal
