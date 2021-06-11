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

#include <sys/resource.h>
#include <sys/time.h>

#include "googletest/gtest/gtest.h"

#include "flare/base/logging.h"
#include "flare/init/override_flag.h"
#include "flare/testing/main.h"

FLARE_OVERRIDE_FLAG(flare_override_rlimit_core, 0);
FLARE_OVERRIDE_FLAG(flare_override_rlimit_core_only_if_less, false);

namespace flare::init {

TEST(OverrideFlag, All) {
  rlimit current;
  FLARE_PCHECK(getrlimit(RLIMIT_CORE, &current) == 0);
  ASSERT_EQ(0, current.rlim_cur);
}

}  // namespace flare::init

FLARE_TEST_MAIN
