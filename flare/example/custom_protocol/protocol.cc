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

#include "flare/example/custom_protocol/protocol.h"

#include <memory>
#include <string>

#include "flare/example/custom_protocol/naive_message.h"

namespace example::naive_proto {

static flare::StreamProtocol::Characteristics characteristics = {
    .name = "Naive protocol"};

const flare::StreamProtocol::Characteristics& Protocol::GetCharacteristics()
    const {
  return characteristics;
}

const flare::MessageFactory* Protocol::GetMessageFactory() const {
  return flare::MessageFactory::null_factory;
}

const flare::ControllerFactory* Protocol::GetControllerFactory() const {
  return flare::ControllerFactory::null_factory;
}

flare::StreamProtocol::MessageCutStatus Protocol::TryCutMessage(
    flare::NoncontiguousBuffer* buffer,
    std::unique_ptr<flare::Message>* message) {
  while (!buffer->Empty()) {
    partial_msg_.append(std::string(buffer->FirstContiguous().data(),
                                    buffer->FirstContiguous().size()));
    buffer->Skip(buffer->FirstContiguous().size());
  }
  auto pos = partial_msg_.find_first_of('\n');
  if (pos == std::string::npos) {
    return MessageCutStatus::NeedMore;
  }
  auto msg = partial_msg_.substr(0, pos);
  *message = std::make_unique<NaiveMessage>(0, msg);
  partial_msg_.erase(0, pos + 1);
  return MessageCutStatus::Cut;
}

bool Protocol::TryParse(std::unique_ptr<flare::Message>* message,
                        flare::Controller* controller) {
  return true;  // We've done everything in `TryCutMessage`.
}

void Protocol::WriteMessage(const flare::Message& message,
                            flare::NoncontiguousBuffer* buffer,
                            flare::Controller* controller) {
  auto p = flare::cast<NaiveMessage>(&message);
  buffer->Append(flare::CreateBufferSlow(p->Body() + '\n'));
}

}  // namespace example::naive_proto
