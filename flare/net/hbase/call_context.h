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

#ifndef FLARE_NET_HBASE_CALL_CONTEXT_H_
#define FLARE_NET_HBASE_CALL_CONTEXT_H_

#include <memory>

#include "protobuf/service.h"

#include "flare/rpc/protocol/controller.h"
#include "flare/net/hbase/proto/rpc.pb.h"

namespace flare {

class HbaseClientController;
class HbaseServerController;

}  // namespace flare

namespace flare::hbase {

// Call context when we're acting as HBase client.
//
// Only `HbaseChannel` & `HbaseProtocol` are aware of this structure.
struct ProactiveCallContext : Controller {
  const google::protobuf::MethodDescriptor* method;
  google::protobuf::Message* response_ptr;
  HbaseClientController* client_controller;

  ProactiveCallContext() { SetRuntimeTypeTo<ProactiveCallContext>(); }
};

// Call context created by `HbaseProtocol`.
struct PassiveCallContext : Controller {
  const google::protobuf::ServiceDescriptor* service;
  const google::protobuf::MethodDescriptor* method;
  std::unique_ptr<google::protobuf::Message> response;
  // Referencing `conn_header` in `HbaseServerProtocol`. The framework should
  // keep the protocol instance alive until all requests have been completed.
  const ConnectionHeader* conn_header;

  PassiveCallContext() { SetRuntimeTypeTo<PassiveCallContext>(); }
};

}  // namespace flare::hbase

#endif  // FLARE_NET_HBASE_CALL_CONTEXT_H_
