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

#ifndef FLARE_IO_NATIVE_STREAM_CONNECTION_H_
#define FLARE_IO_NATIVE_STREAM_CONNECTION_H_

#include <mutex>

#include "flare/base/align.h"
#include "flare/base/buffer.h"
#include "flare/base/maybe_owning.h"
#include "flare/io/descriptor.h"
#include "flare/io/detail/writing_buffer_list.h"
#include "flare/io/stream_connection.h"
#include "flare/io/util/rate_limiter.h"
#include "flare/io/util/stream_io.h"

namespace flare {

// This class represents a TCP connection.
class NativeStreamConnection final : public Descriptor,
                                     public StreamConnection {
 public:
  struct Options {
    // Handler for consuming data and accepting several callbacks.
    MaybeOwning<StreamConnectionHandler> handler;

    // Leave the corresponding field to its default if no rate limitation should
    // be applied.
    //
    // CAUTION: If you override these rate limiter, make sure to composite your
    // own rate limiter with the default ones, otherwise you risk overrunning
    // global rate limit.
    MaybeOwning<RateLimiter> read_rate_limiter{
        non_owning, RateLimiter::GetDefaultRxRateLimiter()};
    MaybeOwning<RateLimiter> write_rate_limiter{
        non_owning, RateLimiter::GetDefaultTxRateLimiter()};

    // Left `nullptr` if TLS is not supported.
    MaybeOwning<AbstractStreamIo> stream_io;

    // Maximum number of not-yet-processed bytes allowed.
    //
    // Set it to `std::numeric_limits<std::size_t>::max()` if you don't want
    // to set a limit (not recommended).
    std::size_t read_buffer_size = 0;  // Default value is invalid.

    // There's no `write_buffer_size`. So long as we're not allowed to block,
    // there's nothing we can do about too many pending writes.
  };

  explicit NativeStreamConnection(Handle fd, Options options);
  ~NativeStreamConnection();

  // Start handshaking with remote peer.
  void StartHandshaking() override;

  // Write `buffer` to the remote side.
  bool Write(NoncontiguousBuffer buffer, std::uintptr_t ctx) override;

  // Restart reading data.
  void RestartRead() override;

  void Stop() override;
  void Join() override;

 private:
  // Note: We only read a handful number of bytes and returns `err != EAGAIN`
  // to the caller (`EventLoop`) so as to not block events of other
  // `Descriptor`s.
  EventAction OnReadable() override;

  // We only write a handful number of bytes on each call.
  EventAction OnWritable() override;

  // An error occurred.
  void OnError(int err) override;

  void OnCleanup(CleanupReason reason) override;

  // Call user's callback to consume read buffer.
  EventAction ConsumeReadBuffer();

  enum class FlushStatus {
    Flushed,
    QuotaExceeded,
    RateLimited,
    SystemBufferSaturated,
    PartialWrite,    // We wrote something out, but the connection is
                     // subsequently closed.
    NothingWritten,  // We wrote nothing out, the connection has been closed (by
                     // remote side) or reached an error state before.
    Error
  };

  FlushStatus FlushWritingBuffer(std::size_t max_bytes);

  AbstractStreamIo::HandshakingStatus DoHandshake(bool from_on_readable);

 private:
  struct HandshakingState {
    std::atomic<bool> done{false};

    std::mutex lock;
    bool need_restart_read{true};  // Not enabled by default.
    bool pending_restart_writes{false};
  };

  Options options_;

  // Describes state of handshaking.
  HandshakingState handshaking_state_;

  // Accessed by reader.
  alignas(hardware_destructive_interference_size)
      NoncontiguousBuffer read_buffer_;

  // Accessed by writers, usually a different thread.
  alignas(hardware_destructive_interference_size) io::detail::WritingBufferList
      writing_buffers_;
};

}  // namespace flare

#endif  // FLARE_IO_NATIVE_STREAM_CONNECTION_H_
