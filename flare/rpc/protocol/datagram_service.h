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

#ifndef FLARE_RPC_PROTOCOL_DATAGRAM_SERVICE_H_
#define FLARE_RPC_PROTOCOL_DATAGRAM_SERVICE_H_

#include <memory>

#include "flare/base/function.h"
#include "flare/rpc/protocol/message.h"
#include "flare/rpc/internal/stream.h"

// Draft, the interface may subject to change.

namespace flare {

// Only messages extracted by `DatagramProtocol` is tried on `DatagramService`.
class DatagramService {
 public:
  virtual ~DatagramService() = default;

  // FIXME: We're using the same names as `StreamService`, this likely will
  // complicate implementation's life if they're inheriting from both
  // interfaces. (But why would they do this in the first place?)

  enum class ProcessingStatus {
    // Everything worked as intended. The `message` will be freed by the
    // framework. If any reply should be made, it is already sent into `from`
    // by the implementation.
    //
    // The implementation may also return this even if it dropped the `message`
    // (e.g., when we're overloaded.)
    Processed,

    // This status is returned if the implementation is not able to handle this
    // message. (e.g., `message` is not the type the implementation is
    // expecting.) If this status is returned, the framework will try next
    // `StreamService` with the same message.
    NotSupported,

    // This status indicates the message is recognized, but it not processed as
    // it's (likely) corrupted. The packet is dropped.
    Corrupted,
  };

  // Called outside of event loop's workers. Blocking is acceptable.
  //
  // TODO(luobogao): We may want to pass in a `Context` here for passing stuffs
  // such as "time of arrival" / "peer address" / etc.
  virtual ProcessingStatus TryProcessMessage(
      std::unique_ptr<Message> message,
      Function<bool(const Message&)> writer) = 0;

  virtual ProcessingStatus TryProcessStream(
      AsyncStreamReader<std::unique_ptr<Message>> stream,
      Function<bool(const Message&)> writer) = 0;

  virtual void Stop() = 0;
  virtual void Join() = 0;
};

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_DATAGRAM_SERVICE_H_
