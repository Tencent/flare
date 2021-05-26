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

#ifndef FLARE_RPC_BUILTIN_NETWORK_LOCATION_HTTP_FILTER_H_
#define FLARE_RPC_BUILTIN_NETWORK_LOCATION_HTTP_FILTER_H_

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "flare/base/function.h"
#include "flare/base/internal/hash_map.h"
#include "flare/rpc/builtin/detail/uri_matcher.h"
#include "flare/rpc/http_filter.h"

namespace flare {

namespace detail {

// Implementation detail. Both filters below are implemented in term of this
// class.
class NetworkLocationHttpFilterImpl : public HttpFilter {
 public:
  explicit NetworkLocationHttpFilterImpl(detail::UriMatcher&& matcher)
      : uri_matcher_(std::move(matcher)) {}

  void InitializePeers(const std::vector<std::string>& endpoints);

  Action OnFilter(HttpRequest* request, HttpResponse* response,
                  HttpServerContext* context) override;

 protected:
  // Internally we use binary representation of IP address. This helps in:
  //
  // - Avoiding representational ambiguity;
  // - Boosting comparision performance.
  using LocationRef = std::pair<sa_family_t, std::string_view>;

  // Note that only network address is checked, "port" is silently ignored.
  bool IsAddressHit(const Endpoint& ep);

  // Get `LocationRef` from an endpoint.
  std::optional<LocationRef> TryGetLocationRef(const Endpoint& ep);

  // Implemented by sub-class to test if `endpoint` should be allowed to access
  // us.
  virtual bool VerifyPeer(const Endpoint& endpoint) = 0;

 private:
  detail::UriMatcher uri_matcher_;
  std::vector<Endpoint> peer_storage_;
  internal::HashMap<LocationRef, bool> entries_;  // `value` is ignored.
};

}  // namespace detail

// Filters request based on requester's IP. Only IPs matching the list are
// allowed.
class NetworkLocationAllowOnHitHttpFilter
    : public detail::NetworkLocationHttpFilterImpl {
 public:
  explicit NetworkLocationAllowOnHitHttpFilter(
      const std::vector<std::string>& allowing,
      detail::UriMatcher uri_matcher = {});

 protected:
  bool VerifyPeer(const Endpoint& endpoint) override {
    return IsAddressHit(endpoint);
  }
};

// Filters request based on requester's IP. Only IPs do NOT match the list are
// allowed.
class NetworkLocationBlockOnHitHttpFilter
    : public detail::NetworkLocationHttpFilterImpl {
 public:
  explicit NetworkLocationBlockOnHitHttpFilter(
      const std::vector<std::string>& blocking,
      detail::UriMatcher uri_matcher = {});

 protected:
  bool VerifyPeer(const Endpoint& endpoint) override {
    return !IsAddressHit(endpoint);
  }
};

}  // namespace flare

#endif  // FLARE_RPC_BUILTIN_NETWORK_LOCATION_HTTP_FILTER_H_
