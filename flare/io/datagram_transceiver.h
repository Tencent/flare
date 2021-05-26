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

#ifndef FLARE_IO_DATAGRAM_TRANSCEIVER_H_
#define FLARE_IO_DATAGRAM_TRANSCEIVER_H_

#include <cinttypes>

#include "flare/base/buffer.h"
#include "flare/base/net/endpoint.h"

namespace flare {

class DatagramTransceiver;

// Handler of datagrams.
class DatagramTransceiverHandler {
 public:
  virtual ~DatagramTransceiverHandler() = default;

  // Called by `DatagramTransceiver`.
  virtual void OnAttach(DatagramTransceiver* transceiver) = 0;
  virtual void OnDetach() = 0;

  // Notifies the user that we've sent out all the writes.
  //
  // This one might be needed by streaming rpc for controlling the number
  // of on-fly requests.
  //
  // The name of this method is chosen deliberately different from the
  // one used by `StreamConnectionHandler`, as terminology "buffer" does
  // not suit here well as I see it.
  virtual void OnPendingWritesFlushed() = 0;

  virtual void OnDatagramWritten(std::uintptr_t ctx) = 0;

  enum class DataConsumptionStatus { Consumed, SuppressRead, Error };

  // Called on data arrival by `DatagramTransceiver.
  //
  // Were `SuppressRead` to be returned, `buffer` is treated as processed
  // (i.e., it's dropped on return, and won't be given to the handler again.).
  //
  // Note that since there's no "connection" here, unless you're shutting down
  // the server and don't want to receive more datagrams from this endpoint
  // (local_ip:local_port), returning error is generally not what you want.
  virtual DataConsumptionStatus OnDatagramArrival(NoncontiguousBuffer buffer,
                                                  const Endpoint& addr) = 0;

  // There's an error on the connection.
  //
  // It's safe to destruct `DatagramTransceiver` in this method.
  virtual void OnError() = 0;
};

// This interface defines datagram-based transport.
class DatagramTransceiver {
 public:
  static_assert(sizeof(std::uintptr_t) >= sizeof(std::uint64_t),
                "We use `std::uintptr_t` to pass context around, it'd better "
                "to be at least as large as `std::uint64_t` so we can handle "
                "`correlation_id`s seamlessly.");

  virtual ~DatagramTransceiver() = default;

  // Each `buffer` is sent as a whole datagram.
  virtual bool Write(Endpoint to, NoncontiguousBuffer buffer,
                     std::uintptr_t ctx) = 0;

  // Restart reading data.
  virtual void RestartRead() = 0;

  // Detach the transceiver from the event loop.
  virtual void Stop() = 0;
  virtual void Join() = 0;
};

}  // namespace flare

#endif  // FLARE_IO_DATAGRAM_TRANSCEIVER_H_
