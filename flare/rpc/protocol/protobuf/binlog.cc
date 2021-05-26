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

#include "flare/rpc/protocol/protobuf/binlog.h"

namespace flare::protobuf {

binlog::ProtoPacketDesc WritePacketDesc(const ProtoMessage& msg) {
  binlog::ProtoPacketDesc desc;

  desc.meta = msg.meta.Get();
  if (auto&& body = msg.msg_or_buffer; body.index() == 0) {
    desc.message = &internal::EarlyInitConstant<NoncontiguousBuffer>();
  } else if (body.index() == 1) {
    desc.message = std::get<1>(body).Get();
  } else {
    FLARE_CHECK_EQ(body.index(), 2);
    desc.message = &std::get<2>(body);
  }
  desc.attachment = &msg.attachment;
  return desc;
}

}  // namespace flare::protobuf
