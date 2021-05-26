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

#ifndef FLARE_RPC_INTERNAL_DATAGRAM_CALL_GATE_H_
#define FLARE_RPC_INTERNAL_DATAGRAM_CALL_GATE_H_

#include <chrono>
#include <memory>

#include "flare/base/endpoint.h"
#include "flare/base/maybe_owning.h"
#include "flare/io/datagram_transceiver.h"
#include "flare/rpc/protocol/datagram_protocol.h"
#include "flare/rpc/protocol/message.h"

namespace flare {

// Unlike `StreamCallGate`, there's no "connection" to own for the datagram
// counterpart.
//
// Thread-safe.
class DatagramCallGate : private DatagramTransceiverHandler {
 public:
  // `nullptr` is given had an error occurred. (Implementation note: were
  // detailed error desired, the signature could be changed to accept an
  // `Expected`.)
  using OnCompletion = Function<void(std::unique_ptr<Message>)>;

  struct Options {
    MaybeOwning<DatagramProtocol> protocol;  // Non-owning.
    Endpoint local_endpoint;  // Left to empty if binding to specific
                              // address is not required.
    // DtlsContext dtls_context;
  };

  explicit DatagramCallGate(const Options& options);

  bool FastCall(const Message& m, OnCompletion on_completion,
                std::chrono::steady_clock::time_point timeout);

  // Extended calls are not supported yet.
 private:
  DatagramTransceiver conn_;
};

}  // namespace flare

#endif  // FLARE_RPC_INTERNAL_DATAGRAM_CALL_GATE_H_
