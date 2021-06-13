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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_MOCK_CHANNEL_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_MOCK_CHANNEL_H_

#include "google/protobuf/service.h"

namespace flare::protobuf::detail {

// Interface of RPC mock channel.
class MockChannel {
 public:
  virtual ~MockChannel() = default;

  virtual void CallMethod(const MockChannel* self,
                          const google::protobuf::MethodDescriptor* method,
                          google::protobuf::RpcController* controller,
                          const google::protobuf::Message* request,
                          google::protobuf::Message* response,
                          google::protobuf::Closure* done) = 0;
};

// Adapts `MockChannel` to `google::protobuf::RpcChannel` for easier use.
class MockChannelAdapter : public google::protobuf::RpcChannel {
 public:
  explicit MockChannelAdapter(MockChannel* channel) : channel_(channel) {}

  void CallMethod(const google::protobuf::MethodDescriptor* method,
                  google::protobuf::RpcController* controller,
                  const google::protobuf::Message* request,
                  google::protobuf::Message* response,
                  google::protobuf::Closure* done) override {
    return channel_->CallMethod(nullptr, method, controller, request, response,
                                done);
  }

 private:
  MockChannel* channel_;
};

}  // namespace flare::protobuf::detail

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_MOCK_CHANNEL_H_
