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

#include "flare/rpc/protocol/protobuf/service_method_locator.h"

#include <set>

#include "googletest/gtest/gtest.h"

#include "flare/testing/main.h"
#include "flare/testing/echo_service.flare.pb.h"

namespace flare::protobuf {

inline constexpr struct FancyProtocol {
  using MethodKey = const void*;  // Pointer to method descriptor.
} fancy;

struct Dummy : testing::EchoService {
} dummy;

auto service = dummy.GetDescriptor();

std::set<std::string> server_methods;

void ServerAddCallback(const google::protobuf::MethodDescriptor* method) {
  server_methods.insert(method->full_name());
  ServiceMethodLocator::Instance()->RegisterMethod(fancy, method, method);
}

void ServerRemoveCallback(const google::protobuf::MethodDescriptor* method) {
  server_methods.erase(method->full_name());
  ServiceMethodLocator::Instance()->DeregisterMethod(fancy, method);
}

TEST(ServiceMethodLocator, ServerSide) {
  ServiceMethodLocator::Instance()->RegisterMethodProvider(
      ServerAddCallback, ServerRemoveCallback);
  ServiceMethodLocator::Instance()->AddService(service);
  std::set<std::string> expected;
  for (int i = 0; i != service->method_count(); ++i) {
    expected.insert(service->method(i)->full_name());
  }
  ASSERT_EQ(expected, server_methods);
  ASSERT_EQ(service->method(0)->full_name(),
            ServiceMethodLocator::Instance()
                ->TryGetMethodDesc(fancy, service->method(0))
                ->normalized_method_name);
  ServiceMethodLocator::Instance()->DeleteService(service);
  ASSERT_TRUE(server_methods.empty());
}

}  // namespace flare::protobuf

FLARE_TEST_MAIN
