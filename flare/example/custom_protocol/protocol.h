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

#ifndef FLARE_EXAMPLE_CUSTOM_PROTOCOL_PROTOCOL_H_
#define FLARE_EXAMPLE_CUSTOM_PROTOCOL_PROTOCOL_H_

#include <memory>
#include <string>

#include "flare/rpc/protocol/stream_protocol.h"

namespace example::naive_proto {

class Protocol : public flare::StreamProtocol {
 public:
  const Characteristics& GetCharacteristics() const override;

  const flare::MessageFactory* GetMessageFactory() const override;
  const flare::ControllerFactory* GetControllerFactory() const override;

  MessageCutStatus TryCutMessage(
      flare::NoncontiguousBuffer* buffer,
      std::unique_ptr<flare::Message>* message) override;

  bool TryParse(std::unique_ptr<flare::Message>* message,
                flare::Controller* controller) override;

  void WriteMessage(const flare::Message& message,
                    flare::NoncontiguousBuffer* buffer,
                    flare::Controller* controller) override;

 private:
  std::string partial_msg_;
};

}  // namespace example::naive_proto

#endif  // FLARE_EXAMPLE_CUSTOM_PROTOCOL_PROTOCOL_H_
