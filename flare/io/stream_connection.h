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

#ifndef FLARE_IO_STREAM_CONNECTION_H_
#define FLARE_IO_STREAM_CONNECTION_H_

#include <cinttypes>

#include "flare/base/buffer.h"

namespace flare {

class StreamConnection;

// This class handles I/O events.
class StreamConnectionHandler {
 public:
  virtual ~StreamConnectionHandler() = default;

  // Called by `StreamConnection` on initialization.
  virtual void OnAttach(StreamConnection* conn) = 0;

  // Called by `StreamConnection`. Note that however, the time by which this
  // method is called is NOT well-defined, you'd better off ignoring it
  // completely.
  virtual void OnDetach() = 0;

  // Notifies the user that we've sent out all the writes. It's called by
  // `StreamConnection` if the write buffer is drained.
  //
  // This one might be needed by streaming rpc for controlling the number
  // of on-fly requests.
  //
  // Please note that this method is called when *there's no pending writes
  // in `StreamConnection`, it's well possible that the underlying OS is
  // still buffering some data.*
  virtual void OnWriteBufferEmpty() = 0;

  // Notifies the user that a write operation has been performed.
  //
  // CAUTION: If the connectino breaks before your data has been written, you
  // won't receive an `OnDataWritten` for your `ctx`.
  //
  // `ctx` passed in is the same as the one passed to `StreamConnection::Write`.
  virtual void OnDataWritten(std::uintptr_t ctx) = 0;

  enum class DataConsumptionStatus { Ready, SuppressRead, Error };

  // Called on data arrival by `StreamConnection`.
  //
  // Note:
  //
  // 1. The implementation is only expected to *consume* data from the head of
  //    the `buffer` (e.g., it may not append new buffers to `buffer`).
  //
  // 2. It's treated as an error if `Ready` is returned and there's still
  //    `read_buffer_size` bytes left in `buffer`. In this case, the connection
  //    will be closed, and `OnError` will be called immediately (so that the
  //    user need not to test for this condition explicitly.).
  //
  // 3. If `SuppressRead` is returned, it's user's responsibility to reenable
  //    read when appropriate by calling `StreamConnection::RestartRead`.
  virtual DataConsumptionStatus OnDataArrival(NoncontiguousBuffer* buffer) = 0;

  // The remote side has closed the connection.
  //
  // On called, the connection has been removed from event loop, and all pending
  // (if any) tasks fired by the connection itself has completed. It's
  // explicitly allowed to destroy the connection object in this callback.
  virtual void OnClose() = 0;

  // There's an error on the connection.
  //
  // On called, the connection has been removed from event loop, and all pending
  // (if any) tasks fired by the connection itself has completed. It's
  // explicitly allowed to destroy the connection object in this callback.
  //
  // Note that `errno` is not available when `OnError` is called even if there
  // was one.
  virtual void OnError() = 0;
};

// This interface defines a byte-oriented connection. It's upto upper layer
// protocol to determine packet boundary.
class StreamConnection {
 public:
  static_assert(sizeof(std::uintptr_t) >= sizeof(std::uint64_t),
                "We use `std::uintptr_t` to pass context around, it'd better "
                "to be at least as large as `std::uint64_t` so we can handle "
                "`correlation_id`s seamlessly.");

  virtual ~StreamConnection() = default;

  // Start handshaking with remote peer. This method must be called after adding
  // the connection to event loop.
  //
  // Failure is reported via `StreamConnectionHandler::OnError()`.
  virtual void StartHandshaking() = 0;

  // The implementation may consolidate multiple writes into a single one, or
  // split a single write into multiple ones. Because this is a stream-oriented
  // connection, this shouldn't hurt.
  //
  // `ctx` can be used to pass context between this call and `StreamConnection-
  // Handler`.
  //
  // Returns: `true` if the operation has been performed or queued.
  //
  //          `false` if the operation is not performed (and won't be performed
  //          in the future) at all. In this case the user may safely resend the
  //          `buffer` (presumably via a different connection) without worrying
  //          about multiple copied being sent.
  virtual bool Write(NoncontiguousBuffer buffer, std::uintptr_t ctx) = 0;

  // Restart reading data.
  //
  // In case there's an executing `OnDataArrival` being about to return
  // `Suppress`, this method is guaranteed to eliminate that suppression.
  virtual void RestartRead() = 0;

  // Detach the connection from its event loop.
  virtual void Stop() = 0;

  // Wait for active operations to complete.
  virtual void Join() = 0;
};

}  // namespace flare

#endif  // FLARE_IO_STREAM_CONNECTION_H_
