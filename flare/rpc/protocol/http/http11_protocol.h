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

#ifndef FLARE_RPC_PROTOCOL_HTTP_HTTP11_PROTOCOL_H_
#define FLARE_RPC_PROTOCOL_HTTP_HTTP11_PROTOCOL_H_

#include <memory>

#include "flare/rpc/protocol/http/buffer_io.h"
#include "flare/rpc/protocol/stream_protocol.h"

namespace flare {

class HttpHeaders;

}  // namespace flare

namespace flare::http {

// HTTP/1.0 is also handled by this one.
class Http11Protocol : public flare::StreamProtocol {
 public:
  explicit Http11Protocol(bool server_side) : server_side_(server_side) {}

  const Characteristics& GetCharacteristics() const override;
  const MessageFactory* GetMessageFactory() const override;
  const ControllerFactory* GetControllerFactory() const override;
  MessageCutStatus TryCutMessage(NoncontiguousBuffer* buffer,
                                 std::unique_ptr<Message>* message) override;
  bool TryParse(std::unique_ptr<Message>* message,
                Controller* controller) override;
  void WriteMessage(const Message& message, NoncontiguousBuffer* buffer,
                    Controller* controller) override;

 private:
  bool server_side_;
  HeaderBlock parsed_header_block_;
};

}  // namespace flare::http

#endif  // FLARE_RPC_PROTOCOL_HTTP_HTTP11_PROTOCOL_H_
