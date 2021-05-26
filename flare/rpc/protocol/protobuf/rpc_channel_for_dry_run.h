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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CHANNEL_FOR_DRY_RUN_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CHANNEL_FOR_DRY_RUN_H_

#include <string>

#include "thirdparty/protobuf/service.h"

namespace flare {

namespace protobuf {

class RpcChannelForDryRun : public google::protobuf::RpcChannel {
 public:
  bool Open(const std::string& uri);

  void CallMethod(const google::protobuf::MethodDescriptor* method,
                  google::protobuf::RpcController* controller,
                  const google::protobuf::Message* request,
                  google::protobuf::Message* response,
                  google::protobuf::Closure* done) override;

 private:
  std::string uri_;
};

}  // namespace protobuf

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CHANNEL_FOR_DRY_RUN_H_
