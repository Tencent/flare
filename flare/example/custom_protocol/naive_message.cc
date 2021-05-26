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

#include "flare/example/custom_protocol/naive_message.h"

#include <utility>

namespace example::naive_proto {

NaiveMessage::NaiveMessage() { SetRuntimeTypeTo<NaiveMessage>(); }

NaiveMessage::NaiveMessage(std::uint64_t cid, std::string msg)
    : correlation_id_(cid), body_(std::move(msg)) {
  SetRuntimeTypeTo<NaiveMessage>();
}

std::uint64_t NaiveMessage::GetCorrelationId() const noexcept {
  return correlation_id_;
}

flare::Message::Type NaiveMessage::GetType() const noexcept {
  return Type::Single;
}

std::string NaiveMessage::Body() const { return body_; }

}  // namespace example::naive_proto
