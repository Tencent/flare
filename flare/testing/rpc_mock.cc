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

#include "flare/testing/rpc_mock.h"

#include <string>

#include "flare/base/internal/lazy_init.h"
#include "flare/init/on_init.h"
#include "flare/rpc/protocol/protobuf/rpc_meta.pb.h"
#include "flare/rpc/rpc_channel.h"

namespace flare::testing::detail {

FLARE_ON_INIT(0 /* doesn't matter */, [] {
  RpcChannel::RegisterMockChannel(internal::LazyInit<MockRpcChannel>());
});

MockRpcChannel::MockRpcChannel() {
  // This can't be done at global initialization time due to static global
  // variable initialization order fiasco.
  ::testing::Mock::AllowLeak(this);
}

void MockRpcChannel::GMockActionReturn(const GMockActionArguments& arguments,
                                       const google::protobuf::Message& value) {
  std::get<3>(arguments)->CopyFrom(value);
  rpc::RpcMeta meta;
  meta.mutable_response_meta()->set_status(rpc::STATUS_SUCCESS);
  RunCompletionWith(
      flare::down_cast<RpcClientController>(std::get<1>(arguments)), meta,
      std::get<4>(arguments));
}

void MockRpcChannel::GMockActionReturn(const GMockActionArguments& arguments,
                                       rpc::Status status) {
  return GMockActionReturn(arguments, Status{status});
}

void MockRpcChannel::GMockActionReturn(const GMockActionArguments& arguments,
                                       const std::string& desc) {
  return GMockActionReturn(arguments, Status{rpc::STATUS_FAILED, desc});
}

void MockRpcChannel::GMockActionReturn(const GMockActionArguments& arguments,
                                       rpc::Status status,
                                       const std::string& desc) {
  return GMockActionReturn(arguments, Status{status, desc});
}

void MockRpcChannel::GMockActionReturn(const GMockActionArguments& arguments,
                                       Status status) {
  rpc::RpcMeta meta;
  auto&& resp_meta = *meta.mutable_response_meta();
  resp_meta.set_status(status.code());
  resp_meta.set_description(status.message());
  RunCompletionWith(
      flare::down_cast<RpcClientController>(std::get<1>(arguments)), meta,
      std::get<4>(arguments));
}

void MockRpcChannel::RunCompletionWith(RpcClientController* ctlr,
                                       const rpc::RpcMeta& meta,
                                       google::protobuf::Closure* done) {
  ctlr->SetCompletion(done ? done : flare::NewCallback([] {}));
  ctlr->NotifyCompletion(Status(meta.response_meta().status(),
                                meta.response_meta().description()));
}

void MockRpcChannel::CopyAttachment(const RpcClientController& from,
                                    RpcServerController* to) {
  to->SetRequestAttachment(from.GetRequestAttachment());
}

void MockRpcChannel::CopyAttachment(const RpcServerController& from,
                                    RpcClientController* to) {
  to->SetResponseAttachment(from.GetResponseAttachment());
}

}  // namespace flare::testing::detail
