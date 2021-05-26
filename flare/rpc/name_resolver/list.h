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

#ifndef FLARE_RPC_NAME_RESOLVER_LIST_H_
#define FLARE_RPC_NAME_RESOLVER_LIST_H_

#include <string>
#include <vector>

#include "flare/rpc/name_resolver/name_resolver.h"
#include "flare/rpc/name_resolver/name_resolver_impl.h"

namespace flare::name_resolver {

// name e.g.: 192.0.2.1:80,192.0.2.2:8080,[2001:db8::1]:8088,www.qq.com:443
//
// IP (v4 / v6) and domain.
class List : public NameResolverImpl {
 public:
  List();

 private:
  bool CheckValid(const std::string& name) override;

  bool GetRouteTable(const std::string& name, const std::string& old_signature,
                     std::vector<Endpoint>* new_address,
                     std::string* new_signature) override;
};

}  // namespace flare::name_resolver

#endif  // FLARE_RPC_NAME_RESOLVER_LIST_H_
