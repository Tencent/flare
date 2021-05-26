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

#ifndef FLARE_RPC_PROTOCOL_DATAGRAM_PROTOCOL_H_
#define FLARE_RPC_PROTOCOL_DATAGRAM_PROTOCOL_H_

#include <memory>
#include <string>

#include "flare/base/buffer.h"
#include "flare/io/datagram_transceiver.h"
#include "flare/rpc/protocol/message.h"

namespace flare {

// Each instance of this class is bound to exactly one datagram endpoint.
//
// Therefore, the implementation is permitted to cache whatever it sees
// suitable in its internal state about the endpoint it's on.
class DatagramProtocol {
 public:
  virtual ~DatagramProtocol() = default;

  enum class MessageParseStatus {
    // The message is successfully translated.
    Success,

    // The packet is corrupt and should be ignored.
    Drop,

    // Error occurred. The endpoint **will be closed**.
    Error
  };

  struct Characteristics {
    // Name of the protocol. For display purpose only.
    std::string name;
  };

  virtual const Characteristics& GetCharacteristics() const = 0;

  // Called upon attaching to / detaching from `DatagramTransceiver`.
  virtual void OnAttachingTransceiver(DatagramTransceiver* transceiver) = 0;
  virtual void OnDetachingTransceiver() = 0;

  // Parse `buffer` into message.
  virtual MessageParseStatus TryParse(const NoncontiguousBuffer& buffer,
                                      std::unique_ptr<Message>* message) = 0;

  // Serialize `message` to buffer.
  virtual void WriteMessage(const Message& message,
                            NoncontiguousBuffer* buffer) = 0;
};

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_DATAGRAM_PROTOCOL_H_
