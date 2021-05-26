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

namespace flare::protobuf {

void ServiceMethodLocator::RegisterMethodProvider(
    LocatorProviderCallback init, LocatorProviderCallback fini) {
  providers_.emplace_back(std::move(init), std::move(fini));
}

void ServiceMethodLocator::AddService(
    const google::protobuf::ServiceDescriptor* service_desc) {
  std::unique_lock lk(services_lock_);
  if (services_[service_desc]++) {
    return;  // Was there.
  }
  lk.unlock();

  for (int i = 0; i != service_desc->method_count(); ++i) {
    auto method = service_desc->method(i);
    for (auto&& e : providers_) {
      e.first(method);
    }
  }
}

void ServiceMethodLocator::DeleteService(
    const google::protobuf::ServiceDescriptor* service_desc) {
  std::unique_lock lk(services_lock_);
  if (--services_[service_desc]) {
    return;  // Was there.
  }
  lk.unlock();

  for (int i = 0; i != service_desc->method_count(); ++i) {
    auto method = service_desc->method(i);
    for (auto&& e : providers_) {
      e.second(method);
    }
  }
}

std::vector<const google::protobuf::ServiceDescriptor*>
ServiceMethodLocator::GetAllServices() const {
  std::unique_lock lk(services_lock_);
  std::vector<const google::protobuf::ServiceDescriptor*> services;
  for (auto&& service : services_) {
    services.push_back(service.first);
  }
  return services;
}

ServiceMethodLocator::ServiceMethodLocator() = default;

}  // namespace flare::protobuf
