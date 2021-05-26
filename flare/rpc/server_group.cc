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

#include "flare/rpc/server_group.h"

#include <memory>
#include <utility>
#include <vector>

#include "flare/rpc/server.h"

namespace flare {

ServerGroup::ServerGroup() = default;
ServerGroup::~ServerGroup() = default;

void ServerGroup::AddServer(
    Endpoint listen_on, const std::vector<std::string>& protocols,
    MaybeOwningArgument<google::protobuf::Service> service) {
  auto server = AddServer();
  server->AddProtocols(protocols);
  server->AddService(std::move(service));
  server->ListenOn(listen_on);
}

Server* ServerGroup::AddServer() {
  servers_.push_back(std::make_unique<Server>());
  return servers_.back().Get();
}

void ServerGroup::AddServer(MaybeOwningArgument<Server> server) {
  servers_.push_back(std::move(server));
}

void ServerGroup::Start() {
  for (auto&& e : servers_) {
    e->Start();
  }
}

void ServerGroup::Stop() {
  for (auto&& e : servers_) {
    e->Stop();
  }
}

void ServerGroup::Join() {
  for (auto&& e : servers_) {
    e->Join();
  }
}

}  // namespace flare
