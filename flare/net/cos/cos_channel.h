// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_NET_COS_COS_CHANNEL_H_
#define FLARE_NET_COS_COS_CHANNEL_H_

// clang-format off
#include "flare/net/cos/channel.h"
// clang-format on

#include "flare/rpc/message_dispatcher/message_dispatcher.h"

namespace flare::cos {

// This channel interacts with our HTTP engine.
class CosChannel : public Channel {
 public:
  // If you're using Polaris address to access COS, call this method.
  bool OpenPolaris(const std::string& polaris_addr);

  void Perform(const cos::Channel* self, const CosOperation& op,
               CosOperationResult* result, const CosTask::Options& options,
               std::chrono::nanoseconds timeout,
               Function<void(Status)>* done) override;

 private:
  std::unique_ptr<MessageDispatcher> dispatcher_;
};

}  // namespace flare::cos

#endif  // FLARE_NET_COS_COS_CHANNEL_H_
