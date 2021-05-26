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
#include "flare/rpc/name_resolver/name_resolver_impl.h"

#include <future>
#include <thread>

#include "thirdparty/googletest/gtest/gtest.h"

using namespace std::literals;

namespace flare::name_resolver {

class MockNs : public NameResolverImpl {
 public:
  MockNs() : NameResolverImpl() { updater_ = GetUpdater(); }
  void SetNewAddress(std::vector<Endpoint> new_addr) {
    std::scoped_lock _(lock_);
    address_ = std::move(new_addr);
  }

 private:
  bool GetRouteTable(const std::string& name, const std::string& old_signature,
                     std::vector<Endpoint>* new_address,
                     std::string* new_signature) override {
    std::scoped_lock _(lock_);
    *new_address = address_;
    return true;
  }

 private:
  std::mutex lock_;
  std::vector<Endpoint> address_;
};

class MockFailNs : public MockNs {
 private:
  bool CheckValid(const std::string& name) override { return false; }
};

TEST(NsImplTest, TestAll) {
  MockNs mock_ns;
  MockFailNs mock_fail_ns;
  const std::string fake_ns = "deadbeef";
  mock_ns.SetNewAddress({EndpointFromIpv4("127.0.0.1", 12345),
                         EndpointFromIpv4("192.0.2.1", 12000)});
  auto view = mock_ns.StartResolving("");
  ASSERT_FALSE(!!view);
  view = mock_fail_ns.StartResolving(fake_ns);
  ASSERT_FALSE(!!view);
  view = mock_ns.StartResolving(fake_ns);
  ASSERT_TRUE(!!view);
  std::vector<std::future<std::int64_t>> versions;
  for (int i = 0; i < 3; ++i) {
    versions.push_back(
        std::async(std::launch::async, [&]() { return view->GetVersion(); }));
  }
  for (auto&& fur : versions) {
    fur.wait();
  }
  EXPECT_NE(std::find_if(versions.begin(), versions.end(),
                         [](auto&& fur) { return fur.get() == 1; }),
            versions.end());
  std::vector<Endpoint> resolved_address;
  view->GetPeers(&resolved_address);
  ASSERT_EQ(2, resolved_address.size());
  auto first_addr = resolved_address[0].ToString();
  auto second_addr = resolved_address[1].ToString();
  EXPECT_TRUE(
      (first_addr == "127.0.0.1:12345" && second_addr == "192.0.2.1:12000") ||
      (first_addr == "192.0.2.1:12000" && second_addr == "127.0.0.1:12345"));
  // Update
  mock_ns.SetNewAddress({EndpointFromIpv4("192.0.2.3", 12345)});
  while (view->GetVersion() != 2) {
    std::this_thread::sleep_for(1s);
  }
  view->GetPeers(&resolved_address);
  ASSERT_EQ(1, resolved_address.size());
  EXPECT_EQ("192.0.2.3:12345", resolved_address[0].ToString());
}

}  // namespace flare::name_resolver
