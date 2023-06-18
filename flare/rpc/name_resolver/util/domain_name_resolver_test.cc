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

#include "flare/rpc/name_resolver/util/domain_name_resolver.h"

#include "gtest/gtest.h"

namespace flare::name_resolver::util {

TEST(ResolveDomain, ResolveDomain) {
  std::vector<Endpoint> addresses;
  if (ResolveDomain("www.qq.com", 443, &addresses)) {
    std::cout << "Query success:\nIP:";
    for (size_t i = 0; i < addresses.size(); ++i)
      std::cout << " " << addresses[i].ToString();
    std::cout << "\n";
  } else {
    std::cout << "Query error: \n";
  }
}

TEST(ResolveDomain, Invalid) {
  std::vector<Endpoint> addresses;
  EXPECT_FALSE(ResolveDomain("non-exist", 0, &addresses));
  EXPECT_FALSE(ResolveDomain("non-exist.domain", 0, &addresses));
}

TEST(ResolveDomain, ResolveWithServers) {
  std::vector<Endpoint> addresses;
  ASSERT_TRUE(ResolveDomain("example.com", 1234, &addresses));
}

}  // namespace flare::name_resolver::util
