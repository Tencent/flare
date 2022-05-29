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

#include "flare/io/native/stream_connection.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "flare/base/exposed_var.h"
#include "flare/base/likely.h"
#include "flare/base/logging.h"
#include "flare/base/object_pool.h"
#include "flare/fiber/errno.h"
#include "flare/io/detail/eintr_safe.h"
#include "flare/io/detail/read_at_most.h"
#include "flare/io/util/socket.h"

using namespace std::literals;

namespace flare {

namespace {

// Why not use a `SmallVector` instead? It should be more performant if a small
// number of elements are stored.
struct UintptrVector {
  std::vector<std::uintptr_t> vector;
};

}  // namespace

ExposedMetrics<std::uint64_t, flare::detail::TscToDuration<std::uint64_t>>
    writeout_latency("flare/io/latency/writeout_latency");
ExposedCounter<std::uint64_t> immediate_writeouts(
    "flare/io/immediate_writeouts");
ExposedCounter<std::uint64_t> deferred_writeouts("flare/io/deferred_writeouts");

using HandshakingStatus = AbstractStreamIo::HandshakingStatus;

NativeStreamConnection::NativeStreamConnection(Handle fd, Options options)
    // No event is enabled by default, `Event::Read` is enabled by `DoHandshake`
    // once handshaking is done.
    : Descriptor(std::move(fd), Event{}, "NativeStreamConnection"),
      options_(std::move(options)) {
  CHECK_NE(options.read_buffer_size, 0);
  options_.handler->OnAttach(this);

  if (!options_.stream_io) {
    options_.stream_io = std::make_unique<SystemStreamIo>(Descriptor::fd());
  }
}

NativeStreamConnection::~NativeStreamConnection() {
  // `options_.handler->OnDetach()` is called in `OnCleanup()`, otherwise it
  // might get called after `NativeStreamConnection::Join()`.
  //
  // The other choice would let `Join()` wait for our destruction, but that is
  // too weird.
}

void NativeStreamConnection::StartHandshaking() {
  auto status = DoHandshake(false);
  if (status == HandshakingStatus::Error) {
    FLARE_VLOG(10, "Failed to do handshake with remote peer, bail out.");
    Kill(CleanupReason::HandshakeFailed);
  } else if (status == HandshakingStatus::Success) {
    // Huge success.
  } else if (status == HandshakingStatus::WannaRead) {
    RestartReadIn(0ns);
  } else if (status == HandshakingStatus::WannaWrite) {
    RestartWriteIn(0ns);
  }
}

bool NativeStreamConnection::Write(NoncontiguousBuffer buffer,
                                   std::uintptr_t ctx) {
  ScopedDeferred _([start = ReadTsc()] {
    writeout_latency->Report(TscElapsed(start, ReadTsc()));
  });

  if (FLARE_LIKELY(writing_buffers_.Append(std::move(buffer), ctx))) {
    if (FLARE_UNLIKELY(
            !handshaking_state_.done.load(std::memory_order_acquire))) {
      // Handshaking in progress, we can't write right now.
      std::scoped_lock _(handshaking_state_.lock);
      if (!handshaking_state_.done.load(std::memory_order_acquire)) {
        // Leave a mark. `DoHandshake` will take care of start writing once
        // handeshake has done.
        FLARE_CHECK(!handshaking_state_.pending_restart_writes);
        handshaking_state_.pending_restart_writes = true;
        // Either handshaking process wanna read or write, the buffer is taken
        // care of by `OnReadable()` / `OnWritable()`, so bail out.
        return true;
      } else {
        // Otherwise it's done by the time we grabbed the lock, fall-through.
      }
    }

    // We're the first one to append to the buffer. Start writing then.
    constexpr auto kMaximumBytesPerCall = 1048576;  // 8Mb. ~0.8ms in 10GbE.
    auto status = FlushWritingBuffer(kMaximumBytesPerCall);

    if (status == FlushStatus::SystemBufferSaturated ||
        status == FlushStatus::QuotaExceeded ||
        status == FlushStatus::RateLimited) {
      // For `kSystemBufferSaturated` / `kRateLimited`, once `OnWritable` is
      // called, it will write out the remaining data.
      //
      // For `kQuotaExceeded`, we actually count on `RearmDescriptor` to cause
      // an `OnWritable` event, which should be the case according to
      // [this](https://stackoverflow.com/q/12920243).
      //
      // Note that ideally we should delay call to `RestartWrite()` if
      // `kRateLimited` is returned, nonetheless it won't hurt if we call it
      // immediately.
      RestartWriteIn(0ms);
      deferred_writeouts->Increment();
    } else if (FLARE_LIKELY(status == FlushStatus::Flushed)) {
      immediate_writeouts->Increment();
      options_.handler->OnWriteBufferEmpty();
    } else if (status == FlushStatus::PartialWrite ||
               status == FlushStatus::Error) {
      FLARE_VLOG(10, "Failed to write: {}", static_cast<int>(status));
      Kill(CleanupReason::Error);
    } else if (status == FlushStatus::NothingWritten) {
      // The connection has been closed by the remote side, nothing is written.
      Kill(CleanupReason::Disconnect);
      return false;
    } else {
      FLARE_CHECK(0, "Unexpected status from FlushWritingBuffer: {}.",
                  static_cast<int>(status));
    }
  } else {
    deferred_writeouts->Increment();
  }
  return true;
}

void NativeStreamConnection::RestartRead() { RestartReadIn(0ns); }

void NativeStreamConnection::Stop() { Kill(CleanupReason::UserInitiated); }

void NativeStreamConnection::Join() {
  WaitForCleanup();

  // I don't think this lock should be required TBH. Yet TSan (the one shipped
  // with GCC 8.2) would report a race between acquiring this lock in
  // `Handshake` (write of 1 byte) and `operator delete` (reading `*this`).
  //
  // Call trace shows that when such error is reported, one thread calls
  // `StartHandshaking`, and thereafter, a different thread `Join` on the
  // connection. Indeed there's no synchronization relationship between two
  // threads, so TSan might be right.
  //
  // To comfort TSan, I keep it here.
  std::scoped_lock _(handshaking_state_.lock);
}

Descriptor::EventAction NativeStreamConnection::OnReadable() {
  if (FLARE_UNLIKELY(
          !handshaking_state_.done.load(std::memory_order_acquire))) {
    auto action = DoHandshake(true);
    if (action == HandshakingStatus::Error) {
      FLARE_VLOG(10, "Failed to handshake with remote peer, bailing out.");
      Kill(CleanupReason::Error);
      return EventAction::Leaving;
    } else if (action == HandshakingStatus::WannaRead) {
      return EventAction::Ready;  // Read buffer has been drained, let's try
                                  // again later.
    } else if (action == HandshakingStatus::WannaWrite) {
      RestartWriteIn(0ns);
      return EventAction::Suppress;
    } else {
      FLARE_CHECK(action == HandshakingStatus::Success);
      // Fall-though in this case.
    }
  }
  FLARE_CHECK(handshaking_state_.done.load(std::memory_order_relaxed));

  std::size_t bytes_left = options_.read_rate_limiter->GetQuota();
  bool rate_limited = bytes_left != std::numeric_limits<std::size_t>::max();

  // We might use `readv` if excessive `read` turns out to be a performance
  // bottleneck.
  while (FLARE_LIKELY(bytes_left)) {
    auto bytes_to_read = std::min(
        bytes_left, options_.read_buffer_size - read_buffer_.ByteSize());
    std::size_t bytes_read;
    auto status = io::detail::ReadAtMost(
        bytes_to_read, options_.stream_io.Get(), &read_buffer_, &bytes_read);

    bytes_left -= bytes_read;
    options_.read_rate_limiter->ConsumeBytes(bytes_read);

    // If we've read something, call user's handler first. We do this even if
    // the remote side is shutting down the connection, this is necessary for
    // handling thing such as end-of-stream marker (in some cases the connection
    // is immediately shutdown after sending the marker).
    if (status == io::detail::ReadStatus::Drained ||
        status == io::detail::ReadStatus::PeerClosing ||
        status == io::detail::ReadStatus::MaxBytesRead) {
      // Really read something..
      if (FLARE_LIKELY(read_buffer_.ByteSize())) {
        // Call user's handler.
        if (auto rc = ConsumeReadBuffer();
            FLARE_UNLIKELY(rc != EventAction::Ready)) {
          return rc;
        }
      }

      // If we've already have `read_buffer_size` bytes and the implementation
      // is still not able to extract a packet, signal an error.
      if (FLARE_UNLIKELY(read_buffer_.ByteSize() >=
                         options_.read_buffer_size)) {
        FLARE_VLOG(10, "Read buffer overrun. Killing the connection (fd [{}]).",
                   fd());
        Kill(CleanupReason::Error);
        return EventAction::Leaving;
      }
    }

    if (FLARE_LIKELY(status == io::detail::ReadStatus::Drained)) {
      return EventAction::Ready;
    } else if (status == io::detail::ReadStatus::PeerClosing) {
      Kill(CleanupReason::Disconnect);
      return EventAction::Leaving;
    } else if (status == io::detail::ReadStatus::Error) {
      Kill(CleanupReason::Error);
      return EventAction::Leaving;
    } else if (status == io::detail::ReadStatus::MaxBytesRead) {
      if (FLARE_LIKELY(bytes_left)) {
        // If this is the case, the reason why `MaxBytesRead` is returned is
        // that we'd filled up the read buffer. Given that we've consumed it
        // (otherwise we should've bailed out already before), let's retry.
        continue;
      }

      // Well we're really throttled then.
      FLARE_CHECK_EQ(bytes_left, 0);  // No more quota.
      FLARE_CHECK(rate_limited);  // Otherwise we should have drained system's
                                  // buffer.
      RestartReadIn(1ms);
      return EventAction::Suppress;
    }
    FLARE_UNREACHABLE();
  }

  // This is a really rare case. The reason why we would get here is
  // `bytes_left` was never non-zero (i.e., `GetQuota()` returned zero).
  RestartReadIn(1ms);
  return EventAction::Suppress;
}

Descriptor::EventAction NativeStreamConnection::OnWritable() {
  if (FLARE_UNLIKELY(
          !handshaking_state_.done.load(std::memory_order_acquire))) {
    auto action = DoHandshake(false);
    if (action == HandshakingStatus::Error) {
      FLARE_VLOG(10, "Failed to handshake with remote peer, bailing out.");
      Kill(CleanupReason::Error);
      return EventAction::Leaving;
    } else if (action == HandshakingStatus::WannaWrite) {
      return EventAction::Ready;  // Write buffer full, try again later.
    } else if (action == HandshakingStatus::WannaRead) {
      RestartReadIn(0ns);
      return EventAction::Suppress;
    } else {
      FLARE_CHECK(action == HandshakingStatus::Success);
      // Fall-though in this case.
    }
  }
  FLARE_CHECK(handshaking_state_.done.load(std::memory_order_relaxed));

  auto status = FlushWritingBuffer(std::numeric_limits<std::size_t>::max());
  if (status == FlushStatus::SystemBufferSaturated) {
    return EventAction::Ready;
  } else if (status == FlushStatus::RateLimited) {
    RestartWriteIn(1ms);
    return EventAction::Suppress;
  } else if (status == FlushStatus::Flushed) {
    options_.handler->OnWriteBufferEmpty();
    return EventAction::Suppress;
  } else if (status == FlushStatus::PartialWrite ||
             status == FlushStatus::NothingWritten) {
    Kill(CleanupReason::Disconnect);
    return EventAction::Leaving;
  } else if (status == FlushStatus::Error) {
    Kill(CleanupReason::Error);
    return EventAction::Leaving;
  } else {
    FLARE_CHECK(0, "Unexpected status from FlushWritingBuffer: {}",
                static_cast<int>(status));
  }
}

void NativeStreamConnection::OnError(int err) { Kill(CleanupReason::Error); }

void NativeStreamConnection::OnCleanup(CleanupReason reason) {
  FLARE_CHECK(reason != CleanupReason::None);
  if (reason == CleanupReason::UserInitiated ||
      reason == CleanupReason::Disconnect) {
    options_.handler->OnClose();
  } else {
    options_.handler->OnError();
  }
  options_.handler->OnDetach();
}

Descriptor::EventAction NativeStreamConnection::ConsumeReadBuffer() {
  // Call the user's handler.
  auto action = options_.handler->OnDataArrival(&read_buffer_);
  if (FLARE_LIKELY(action ==
                   StreamConnectionHandler::DataConsumptionStatus::Ready)) {
    return EventAction::Ready;
  } else if (action == StreamConnectionHandler::DataConsumptionStatus::Error) {
    // Do we need warning log here?
    Kill(CleanupReason::Error);
    return EventAction::Leaving;
  } else if (action ==
             StreamConnectionHandler::DataConsumptionStatus::SuppressRead) {
    return EventAction::Suppress;
  }
  FLARE_UNREACHABLE();
}

NativeStreamConnection::FlushStatus NativeStreamConnection::FlushWritingBuffer(
    std::size_t max_bytes) {
  auto bytes_quota =
      std::min(max_bytes, options_.write_rate_limiter->GetQuota());
  bool rate_limited = bytes_quota != max_bytes;
  bool ever_succeeded = false;

  while (bytes_quota) {
    auto ctxs = object_pool::Get<UintptrVector>();
    bool emptied, short_write;
    auto written =
        writing_buffers_.FlushTo(options_.stream_io.Get(), bytes_quota,
                                 &ctxs->vector, &emptied, &short_write);
    if (FLARE_UNLIKELY(written == 0)) {  // The remote side has closed the
                                         // connection.
      return ever_succeeded ? FlushStatus::PartialWrite
                            : FlushStatus::NothingWritten;
    }
    if (FLARE_UNLIKELY(written < 0)) {
      auto err = fiber::GetLastError();
      if (err == EAGAIN || err == EWOULDBLOCK) {
        return FlushStatus::SystemBufferSaturated;
      } else {
        FLARE_VLOG(10, "Cannot write to fd [{}].", fd());
        return ever_succeeded ? FlushStatus::Error
                              : FlushStatus::NothingWritten;
      }
    }
    FLARE_CHECK_LE(written, bytes_quota);

    // Let's update the statistics.
    ever_succeeded = true;
    bytes_quota -= written;
    options_.write_rate_limiter->ConsumeBytes(written);

    // Call user's callbacks.
    for (auto&& e : ctxs->vector) {
      options_.handler->OnDataWritten(e);
    }
    FLARE_CHECK(!(short_write && emptied));
    if (emptied) {
      FLARE_CHECK(!ctxs->vector.empty());
      return FlushStatus::Flushed;
    }
    if (short_write) {
      return FlushStatus::SystemBufferSaturated;
    }
  }

  return rate_limited ? FlushStatus::RateLimited : FlushStatus::QuotaExceeded;
}

AbstractStreamIo::HandshakingStatus NativeStreamConnection::DoHandshake(
    bool from_on_readable) {
  std::scoped_lock _(handshaking_state_.lock);
  if (handshaking_state_.done.load(std::memory_order_acquire)) {
    return HandshakingStatus::Success;
  }

  auto status = options_.stream_io->Handshake();
  if (status == HandshakingStatus::Error) {
    return status;
  } else if (status == HandshakingStatus::Success) {
    if (handshaking_state_.need_restart_read) {
      // The last non-terminal status returned by us was `WannaWrite`, in this
      // case the caller should have suppressed read event. Reenable it then.
      FLARE_CHECK(!from_on_readable);
      RestartReadIn(0ns);
    }
    if (handshaking_state_.pending_restart_writes) {
      // Someone tried to write but suspended as handshake hadn't done at that
      // time, restart that operation then.
      if (from_on_readable) {  // If we're acting on an write event, simply
                               // asking the caller to fallthrough is enough.
        RestartWriteIn(0ns);
      }
    }
    // If `OnReadable` / `OnWritable` (enabled above) comes before we've finally
    // updated `handshaking_state_.done`, they will call us first. In this case,
    // the test at the beginning of this method will see the update (after we've
    // released the lock here) and return `Success` correctly.
    handshaking_state_.done.store(true, std::memory_order_release);
    return status;
  } else if (status == HandshakingStatus::WannaWrite) {
    // Returning `WannaWrite` would make the caller to suppress `Event::Read`.
    // However, read event is always interested once handshake is done. So we
    // leave a mark here, and re-enable read event once handshake is done.
    handshaking_state_.need_restart_read = true;
    return status;
  } else {
    FLARE_CHECK(status == HandshakingStatus::WannaRead);
    handshaking_state_.need_restart_read = false;
    return status;
  }
}

template <>
struct PoolTraits<UintptrVector> {
  static constexpr auto kType = PoolType::MemoryNodeShared;

  // I don't think we need many of them, TBH. In most cases, one for each thread
  // should be sufficient.
  static constexpr auto kLowWaterMark = 128;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 16;
  static constexpr auto kTransferBatchSize = 32;

  static void OnPut(UintptrVector* p) { p->vector.clear(); }
};

}  // namespace flare
