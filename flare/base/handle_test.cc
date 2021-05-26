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

#include "flare/base/handle.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "thirdparty/googletest/gtest/gtest.h"

using namespace std::literals;

namespace flare {

TEST(Handle, InvalidValues) {
  Handle h(0), h2(-1);
  ASSERT_FALSE(h);
  ASSERT_FALSE(h2);
}

TEST(Handle, ValidHandle) {
  int fd = 0;
  {
    Handle h(open("/dev/null", O_WRONLY));
    fd = h.Get();
    ASSERT_EQ(1, write(fd, "1", 1));
    ASSERT_TRUE(h);
  }
  ASSERT_EQ(-1, write(fd, "1", 1));
}

}  // namespace flare
