// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/rpc/builtin/network_location_http_filter.h"

namespace flare {

namespace detail {

void NetworkLocationHttpFilterImpl::InitializePeers(
    const std::vector<std::string>& endpoints) {
  // This must be done prior to constructing `LocationRef`. Inserting into
  // vector may invalidate existing references.
  for (auto&& e : endpoints) {
    std::optional<Endpoint> ep;
    if (e.find_first_of(':') != std::string::npos) {
      // IPv6.
      ep = TryParse<Endpoint>(Format("[{}]:{}", e, 0));
    } else {
      ep = TryParse<Endpoint>(Format("{}:{}", e, 0));
    }
    FLARE_CHECK(ep, "Invalid IP address: {}", e);
    peer_storage_.push_back(*ep);
  }

  for (auto&& e : peer_storage_) {
    auto ref = TryGetLocationRef(e);
    FLARE_CHECK(ref, "Unrecognized address family?");
    entries_[*ref] = true;  // Value is not significant.
  }
}

HttpFilter::Action NetworkLocationHttpFilterImpl::OnFilter(
    HttpRequest* request, HttpResponse* response, HttpServerContext* context) {
  if (!VerifyPeer(context->remote_peer)) {
    GenerateDefaultResponsePage(HttpStatus::Forbidden, response);
    return Action::EarlyReturn;
  }

  return Action::KeepProcessing;
}

bool NetworkLocationHttpFilterImpl::IsAddressHit(const Endpoint& ep) {
  auto ref = TryGetLocationRef(ep);
  if (!ref) {
    // The address family is not supported. It can't be in our list, otherwise
    // how could we initialize the list with an address of such address
    // family?
    return false;
  }
  return entries_.contains(*ref);
}

std::optional<NetworkLocationHttpFilterImpl::LocationRef>
NetworkLocationHttpFilterImpl::TryGetLocationRef(const Endpoint& ep) {
  if (ep.Family() == AF_INET) {
    auto&& addr = ep.UnsafeGet<sockaddr_in>()->sin_addr;
    return LocationRef{
        AF_INET,
        std::string_view(reinterpret_cast<const char*>(&addr), sizeof(addr))};
  } else if (ep.Family() == AF_INET6) {
    auto&& addr = ep.UnsafeGet<sockaddr_in6>()->sin6_addr;
    return LocationRef{
        AF_INET6,
        std::string_view(reinterpret_cast<const char*>(&addr), sizeof(addr))};
  } else {
    return std::nullopt;
  }
}

}  // namespace detail

NetworkLocationAllowOnHitHttpFilter::NetworkLocationAllowOnHitHttpFilter(
    const std::vector<std::string>& allowing, detail::UriMatcher uri_matcher)
    : NetworkLocationHttpFilterImpl(std::move(uri_matcher)) {
  InitializePeers(allowing);
}

NetworkLocationBlockOnHitHttpFilter::NetworkLocationBlockOnHitHttpFilter(
    const std::vector<std::string>& blocking, detail::UriMatcher uri_matcher)
    : NetworkLocationHttpFilterImpl(std::move(uri_matcher)) {
  InitializePeers(blocking);
}

}  // namespace flare
