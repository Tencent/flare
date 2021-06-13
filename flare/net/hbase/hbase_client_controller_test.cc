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

#include "flare/net/hbase/hbase_client_controller.h"

#include "gtest/gtest.h"

#include "flare/testing/main.h"

namespace flare {

TEST(HbaseClientController, Priority) {
  HbaseClientController ctlr;

  ASSERT_EQ(0, ctlr.GetPriority());
  ctlr.SetPriority(100);
  ASSERT_EQ(100, ctlr.GetPriority());
  ctlr.Reset();
  ASSERT_EQ(0, ctlr.GetPriority());
}

}  // namespace flare

FLARE_TEST_MAIN
