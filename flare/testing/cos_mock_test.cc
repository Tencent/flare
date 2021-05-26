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

#include "flare/testing/cos_mock.h"

#include "thirdparty/googletest/gmock/gmock-matchers.h"
#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/net/cos/cos_client.h"
#include "flare/net/cos/ops/object/get_object.h"
#include "flare/testing/main.h"

using namespace std::literals;

using testing::_;

namespace flare::testing {

TEST(CosMock, Fail) {
  CosClient client;
  ASSERT_TRUE(client.Open("mock://...", {}));
  FLARE_EXPECT_COS_OP(GetObject).WillRepeatedly(Return(Status(-1)));
  auto result = client.Execute(CosGetObjectRequest());
  ASSERT_FALSE(result);
  EXPECT_EQ(-1, result.error().code());
}

TEST(CosMock, HandleCosOp) {
  CosClient client;
  ASSERT_TRUE(client.Open("mock://...", {}));
  FLARE_EXPECT_COS_OP(GetObject).WillRepeatedly(HandleCosOp(
      [](const CosGetObjectRequest& req, CosGetObjectResult* result, auto&&) {
        result->bytes = CreateBufferSlow("something");
        return Status();
      }));

  auto result = client.Execute(CosGetObjectRequest());
  ASSERT_TRUE(result);
  EXPECT_EQ("something", FlattenSlow(result->bytes));
}

TEST(CosMock, HandleCosOp2) {
  CosClient client;
  ASSERT_TRUE(client.Open("mock://...", {}));
  FLARE_EXPECT_COS_OP(GetObject).WillRepeatedly(HandleCosOp(
      [](const CosGetObjectRequest& req, CosGetObjectResult* result) {
        result->bytes = CreateBufferSlow("something");
        return Status();
      }));

  auto result = client.Execute(CosGetObjectRequest());
  ASSERT_TRUE(result);
  EXPECT_EQ("something", FlattenSlow(result->bytes));
}

}  // namespace flare::testing

FLARE_TEST_MAIN
