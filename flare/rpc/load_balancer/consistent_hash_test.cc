// Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <string>
#include <unordered_map>
#include <vector>

#include "gtest/gtest.h"

#include "flare/base/net/endpoint.h"
#include "flare/base/random.h"
#include "flare/base/string.h"
#include "flare/rpc/load_balancer/consistent_hash.h"

namespace flare {

TEST(ConsistentHash, Basic) {
  load_balancer::ConsistentHash ch;
  std::vector<Endpoint> endpoints;
  int hosts = 1e2;     // Assume that there have 1e2 hosts.
  int requests = 1e6;  // 1e6 requests should be dispatched in total.
  int average = requests / hosts;  // Average request peer one host.
  double rate = 0.05;              // Error rate.
  for (int i = 0; i < hosts; ++i) {
    Endpoint cur = EndpointFromIpv4(
        Format("{}.{}.{}.{}", Random<std::uint8_t>(), Random<std::uint8_t>(),
               Random<std::uint8_t>(), Random<std::uint8_t>()),
        Random<std::uint16_t>());
    endpoints.push_back(std::move(cur));
  }
  ch.SetPeers(endpoints);
  std::unordered_map<std::string, int> usage;
  for (int i = 0; i < requests; ++i) {
    std::uintptr_t ctx;
    Endpoint result;
    ch.GetPeer(i, &result, &ctx);
    usage[result.ToString()]++;
  }
  int error = 0;
  for (auto& e : usage) {
    std::cout << "Endpoint:" << e.first << " Requests:" << e.second
              << std::endl;
    if (e.second > average) {
      // Each host should receive about `average` requests. Only the cases
      // greater than `average` are counted, otherwise the calculation is
      // repeated.
      error += e.second - average;
    }
  }
  // As long as the error is less than `rate`, we think it's ok.
  // Not always true, does `std::hash` not a better way?
  std::cout << "error=" << error << " requests * rate=" << requests * rate
            << std::endl;
  EXPECT_LE(error, requests * rate);
}

}  // namespace flare
