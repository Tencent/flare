// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/base/delayed_init.h"

#include "googletest/gtest/gtest.h"

using namespace std::literals;

namespace flare {

bool initialized = false;

struct DefaultConstructibleInitialization {
  DefaultConstructibleInitialization() { initialized = true; }
};

struct InitializeCtorArgument {
  explicit InitializeCtorArgument(bool* ptr) { *ptr = true; }
};

TEST(DelayedInit, DefaultConstructible) {
  DelayedInit<DefaultConstructibleInitialization> dc;
  ASSERT_FALSE(initialized);
  dc.Init();
  ASSERT_TRUE(initialized);
}

TEST(DelayedInit, InitializeCtorArgument) {
  bool f = false;
  DelayedInit<InitializeCtorArgument> ica;
  ASSERT_FALSE(f);
  ica.Init(&f);
  ASSERT_TRUE(f);
}

}  // namespace flare
