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

#ifndef FLARE_RPC_SERVER_GROUP_H_
#define FLARE_RPC_SERVER_GROUP_H_

#include <string>
#include <vector>

#include "flare/base/maybe_owning.h"
#include "flare/base/net/endpoint.h"

// Let's keep the header clean. (As a counter-example, `server.h` brings in a
// whole lot of dependencies, which hurt compilation speed badly.)
namespace google::protobuf {

class Service;

}  // namespace google::protobuf

namespace flare {

class Server;

// When you need to host multiple servers in a single process, this class can be
// handy for managing them.
class ServerGroup {
 public:
  ServerGroup();
  ~ServerGroup();

  // If you only need to expose a single Protocol Buffers service, this shortcut
  // can be handy.
  void AddServer(Endpoint listen_on, const std::vector<std::string>& protocols,
                 MaybeOwningArgument<google::protobuf::Service> service);

  // I'm not sure if we should add a shortcut for HTTP servers too.

  // Add a new `Server` instance. You need to call `AddProtocol` / ... to
  // initialize it. Note that you should not call `Start()` on it yourself.
  Server* AddServer();

  // Add a new `Server` instantiated by yourself.
  void AddServer(MaybeOwningArgument<Server> server);

  // Start all servers. No more calls to `AddServer` is allowed since now.
  void Start();

  // Stop & join on all servers.
  void Stop();
  void Join();

  // Non-copyable, non-movable.
  ServerGroup(const ServerGroup&) = delete;
  ServerGroup& operator=(const ServerGroup&) = delete;

 private:
  std::vector<MaybeOwning<Server>> servers_;
};

}  // namespace flare

#endif  // FLARE_RPC_SERVER_GROUP_H_
