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

#include "flare/base/logging.h"
#include "flare/base/string.h"
#include "flare/rpc/name_resolver/util/domain_name_resolver.h"

namespace flare::name_resolver {
namespace {

bool SplitAddr(const std::string_view& addr, std::string* hostname,
               uint16_t* port) {
  auto port_pos = addr.find_last_of(':');
  *hostname = addr.substr(0, port_pos);
  if (auto p = TryParse<std::uint16_t>(addr.substr(port_pos + 1))) {
    *port = *p;
    return true;
  }
  return false;
}

}  // namespace

FLARE_RPC_REGISTER_NAME_RESOLVER("list", List);

List::List() { updater_ = GetUpdater(); }

bool List::CheckValid(const std::string& name) {
  auto addrs = Split(name, ',');
  for (auto&& e : addrs) {
    std::string hostname;
    uint16_t port;
    if (!SplitAddr(e, &hostname, &port)) {
      FLARE_LOG_ERROR("Addr invalid {}", e);
      return false;
    }
    // Check if hostname is ip.
    if (hostname.size() > 2 &&
        (e.find('[') != std::string_view::npos ||
         (isdigit(hostname[0]) && isdigit(hostname.back())))) {
      if (!TryParse<Endpoint>(e)) {
        FLARE_LOG_ERROR("Addr invalid {}", e);
        return false;
      }
    }
  }
  return true;
}

bool List::GetRouteTable(const std::string& name,
                         const std::string& old_signature,
                         std::vector<Endpoint>* new_address,
                         std::string* new_signature) {
  auto addrs = Split(name, ",");
  for (auto&& e : addrs) {
    std::string hostname;
    uint16_t port;
    FLARE_CHECK(SplitAddr(e, &hostname, &port),
                "Addr should already be checked");
    if (hostname.size() > 2 && e.find('[') != std::string_view::npos) {
      // hostname ex : [2001:db8::1]
      new_address->push_back(
          EndpointFromIpv6(hostname.substr(1, hostname.size() - 2), port));
    } else if (hostname.size() > 2 && isdigit(hostname[0]) &&
               isdigit(hostname.back())) {
      new_address->push_back(EndpointFromIpv4(hostname, port));
    } else {
      flare::name_resolver::util::ResolveDomain(hostname, port, new_address);
    }
  }
  return true;
}

}  // namespace flare::name_resolver
