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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_STD_PROTOCOL_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_STD_PROTOCOL_H_

#include "flare/rpc/protocol/stream_protocol.h"

namespace flare::protobuf {

// struct Header {  // All numbers are in network byte order.
//   __le32 magic;  // 'F', 'R', 'P', 'C'
//   __le32 meta_size;  // Size of meta.
//   __le32 msg_size;  // Size of message.
//   __le32 att_size;  // Size of attachment.
// };
//
// Wire format:  [Header][meta][payload][attachment]

class StdProtocol : public StreamProtocol {
 public:
  explicit StdProtocol(bool server_side) : server_side_(server_side) {}

  const Characteristics& GetCharacteristics() const override;

  const MessageFactory* GetMessageFactory() const override;
  const ControllerFactory* GetControllerFactory() const override;

  // Examine `buffer` and extract message.
  MessageCutStatus TryCutMessage(NoncontiguousBuffer* buffer,
                                 std::unique_ptr<Message>* message) override;

  bool TryParse(std::unique_ptr<Message>* message,
                Controller* controller) override;

  // Serialize `message` into `buffer`.
  void WriteMessage(const Message& message, NoncontiguousBuffer* buffer,
                    Controller* controller) override;

 private:
  bool server_side_;
};

}  // namespace flare::protobuf

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_STD_PROTOCOL_H_
