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

#include "flare/rpc/name_resolver/list.h"

#include "gtest/gtest.h"

namespace flare::name_resolver {

TEST(ListNameResolver, StartResolving) {
  auto list_name_resolver_factory = name_resolver_registry.Get("list");
  ASSERT_TRUE(!!list_name_resolver_factory);

  auto list_view = list_name_resolver_factory->StartResolving(
      "192.0.2.1.5:0,192.0.2.2:8080");
  ASSERT_FALSE(!!list_view);

  list_view = list_name_resolver_factory->StartResolving(
      "192.0.2.1:80,192.0.2.2:8080,[2001:db8::1]:8088");
  ASSERT_TRUE(!!list_view);

  EXPECT_EQ(1, list_view->GetVersion());
  std::vector<Endpoint> peers;
  list_view->GetPeers(&peers);
  ASSERT_EQ(3, peers.size());
  ASSERT_EQ("192.0.2.1:80", peers[0].ToString());
  ASSERT_EQ("192.0.2.2:8080", peers[1].ToString());
  ASSERT_EQ("[2001:db8::1]:8088", peers[2].ToString());
  peers.clear();

  list_view = list_name_resolver_factory->StartResolving(
      "192.0.2.1:8080,www.qq.com:80");
  ASSERT_TRUE(list_view != nullptr);

  list_view->GetPeers(&peers);
  ASSERT_GE(peers.size(), 2);

  auto it = std::find_if(peers.begin(), peers.end(), [](auto&& endpoint) {
    return endpoint.ToString() == "192.0.2.1:8080";
  });
  EXPECT_NE(it, peers.end());
}

}  // namespace flare::name_resolver
