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

#include "flare/base/option/key.h"

#include "googletest/gtest/gtest.h"

namespace flare::option {

TEST(Key, MultiKey) {
  MultiKey key(DynamicKey("name"), FixedKey("fixed"));
  SetDynamicKey("name", "dynamic");

  ASSERT_EQ("dynamic", key.GetKeys()[0].Get());
  ASSERT_EQ("fixed", key.GetKeys()[1].Get());
  ASSERT_EQ("dynamic/fixed", key.ToString());
}

TEST(Key, FromStr) {
  Key key("asdf");
  ASSERT_EQ("asdf", key.Get());
}

TEST(Key, FixedKey) {
  Key key(FixedKey("asdf"));
  ASSERT_EQ("asdf", key.Get());
}

TEST(Key, DynamicKey) {
  Key key(DynamicKey("key name"));
  SetDynamicKey("key name", "value");
  ASSERT_EQ("value", key.Get());
}

std::string refee;

TEST(Key, ReferencingKey) {
  refee = "abcd";
  Key key{ReferencingKey(refee)};
  ASSERT_EQ("abcd", key.Get());
}

}  // namespace flare::option
