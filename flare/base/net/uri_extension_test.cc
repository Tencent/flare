// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/base/net/uri.h"

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/string.h"
#include "flare/init/override_flag.h"
#include "flare/testing/main.h"

FLARE_OVERRIDE_FLAG(flare_extension_non_conformant_uri_for_gdt, true);

namespace flare {

TEST(Uri, NonConformantQuery) {
  auto parsed = TryParse<Uri>("http://x.y.z/?a=a&b={'k':'v'}");
  ASSERT_TRUE(parsed);
  EXPECT_EQ("a=a&b={'k':'v'}", parsed->query());
}

}  // namespace flare

FLARE_TEST_MAIN
