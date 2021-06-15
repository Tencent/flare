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

#include "flare/base/option/option_service.h"

#include <iostream>

#include "gflags/gflags.h"
#include "gtest/gtest.h"

#include "flare/base/option.h"

DEFINE_int32(int_test, 1, "");
DEFINE_int64(int64_test, 2, "");
DEFINE_string(string_test, "abc", "");

namespace flare::option {

GflagsOptions<int> opt_int("int_test");
GflagsOptions<std::int64_t> opt_int64("int64_test");
GflagsOptions<std::string> opt_str("string_test");

TEST(OptionService, Dump) {
  InitializeOptions();
  auto options = OptionService::Instance()->Dump();

  std::cout << options.toStyledString() << std::endl;
  ASSERT_EQ(3, options["gflags"].size());
  ASSERT_EQ(1, options["gflags"]["int_test"].asInt());
  ASSERT_EQ(2, options["gflags"]["int64_test"].asInt64());
  ASSERT_EQ("abc", options["gflags"]["string_test"].asString());

  FLAGS_int64_test = 5;
  SynchronizeOptions();
  options = OptionService::Instance()->Dump();
  ASSERT_EQ(5, options["gflags"]["int64_test"].asInt64());

  ShutdownOptions();
}

}  // namespace flare::option
