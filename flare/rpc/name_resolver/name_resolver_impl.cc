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

#include <algorithm>

#include "gflags/gflags.h"

#include "flare/base/logging.h"

DEFINE_int32(flare_name_resolver_update_interval_seconds, 3,
             "NameResolver update interval, in seconds");

using namespace std::literals;

namespace flare::name_resolver {

NameResolverImpl::~NameResolverImpl() {
  if (updater_) {
    updater_->Stop();
  }
}

std::pair<std::shared_ptr<NameResolverImpl::RouteInfo>, bool>
NameResolverImpl::GetRouteInfo(const std::string& name) {
  {
    std::shared_lock lk(name_mutex_);
    if (auto iter = name_route_.find(name); iter != name_route_.end()) {
      return {iter->second, false};
    }
  }
  std::scoped_lock lk(name_mutex_);

  auto [route_iter, ok] =
      name_route_.emplace(name, std::make_shared<RouteInfo>());

  if (ok) {
    UpdateRoute(name, route_iter->second);
  }

  return {route_iter->second, ok};
}

std::unique_ptr<NameResolutionView> NameResolverImpl::StartResolving(
    const std::string& name) {
  FLARE_CHECK(updater_,
              "Default implementation of GetVersion need an updater!");
  if (name.empty() || !CheckValid(name)) {
    return nullptr;
  }
  auto&& [route_info_ptr, register_updater] = GetRouteInfo(name);
  FLARE_CHECK(!!route_info_ptr, "Can't been here");
  if (register_updater) {
    updater_->Register(
        name,
        [=, this, route_info_ptr = route_info_ptr]() {
          UpdateRoute(name, route_info_ptr);
        },
        FLAGS_flare_name_resolver_update_interval_seconds * 1s);
  }
  return std::make_unique<NameResolutionViewImpl>(route_info_ptr);
}

void NameResolverImpl::UpdateRoute(const std::string& name,
                                   std::shared_ptr<RouteInfo> route_info) {
  std::vector<Endpoint> new_address_table;
  std::string old_signature, new_signature;
  if (name_signatures_.find(name) != name_signatures_.end()) {
    old_signature = name_signatures_[name];
  }
  if (!GetRouteTable(name, old_signature, &new_address_table, &new_signature)) {
    return;
  }
  if (!new_signature.empty()) {
    if (old_signature == new_signature) {
      // not changed.
      return;
    } else {
      name_signatures_[name] = new_signature;
    }
  }
  std::sort(new_address_table.begin(), new_address_table.end(),
            [](auto&& left, auto&& right) {
              return left.ToString() < right.ToString();
            });
  std::scoped_lock lk(route_info->route_mutex);
  if (new_address_table != route_info->route_table) {
    route_info->route_table = std::move(new_address_table);
    route_info->version.fetch_add(1, std::memory_order_relaxed);
  }
}

NameResolverUpdater* NameResolverImpl::GetUpdater() {
  // shared by all instances
  static NameResolverUpdater updater;
  return &updater;
}

NameResolutionViewImpl::NameResolutionViewImpl(
    std::shared_ptr<NameResolverImpl::RouteInfo> route)
    : route_(std::move(route)) {}

NameResolutionViewImpl::~NameResolutionViewImpl() {}

std::int64_t NameResolutionViewImpl::GetVersion() {
  return route_->version.load(std::memory_order_relaxed);
}

void NameResolutionViewImpl::GetPeers(std::vector<Endpoint>* addresses) {
  std::scoped_lock lk(route_->route_mutex);
  addresses->assign(route_->route_table.begin(), route_->route_table.end());
}

}  // namespace flare::name_resolver
