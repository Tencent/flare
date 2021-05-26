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

#include "flare/base/internal/lazy_init.h"
#include "flare/init/on_init.h"
#include "flare/net/cos/cos_client.h"

namespace flare::testing::detail {

FLARE_ON_INIT(0 /* doesn't matter */, [] {
  CosClient::RegisterMockChannel(internal::LazyInit<MockCosChannel>());
});

void MockCosChannel::GMockActionReturn(const GMockActionArguments& arguments,
                                       const Status& status) {
  (*std::get<4>(arguments))(status);
}

}  // namespace flare::testing::detail
