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

#include "flare/io/native/datagram_transceiver.h"

#include <limits>
#include <string>
#include <utility>

#include "flare/io/detail/eintr_safe.h"

using namespace std::literals;

namespace flare {

// UNTESTED.

NativeDatagramTransceiver::NativeDatagramTransceiver(Handle fd, Options options)
    : Descriptor(std::move(fd), Event::Read, "NativeDatagramTransceiver"),
      options_(std::move(options)) {
  options_.handler->OnAttach(this);
}

NativeDatagramTransceiver::~NativeDatagramTransceiver() {}

bool NativeDatagramTransceiver::Write(Endpoint to, NoncontiguousBuffer buffer,
                                      std::uintptr_t ctx) {
  constexpr std::size_t kMaximumWritesPerCall = 64;  // Number of syscalls.

  if (write_buffer_.Append(std::move(to), std::move(buffer), ctx)) {
    auto rc = FlushWritingBuffer(kMaximumWritesPerCall);

    if (rc == FlushStatus::SystemBufferSaturated ||
        rc == FlushStatus::QuotaExceeded) {
      RestartWriteIn(0ms);
    } else if (rc == FlushStatus::Flushed) {
      options_.handler->OnPendingWritesFlushed();
    } else if (rc == FlushStatus::PartialWrite || rc == FlushStatus::Error) {
      FLARE_LOG_WARNING_EVERY_SECOND("Failed to write: {}",
                                     static_cast<int>(rc));
      // The error should be find-able by EPOLLERR. Nothing here.
    } else if (rc == FlushStatus::NothingWritten) {
      // The connection has been closed by the remote side, nothing is written.
      return false;
    } else {
      FLARE_CHECK(0, "Unexpected status from FlushWritingBuffer: {}.",
                  static_cast<int>(rc));
    }
  }
  return true;
}

void NativeDatagramTransceiver::RestartRead() { RestartReadIn(0ms); }

void NativeDatagramTransceiver::Stop() { Kill(CleanupReason::UserInitiated); }

void NativeDatagramTransceiver::Join() { WaitForCleanup(); }

Descriptor::EventAction NativeDatagramTransceiver::OnReadable() {
  while (true) {
    thread_local std::string buffer(options_.maximum_packet_size, 0);
    EndpointRetriever er;

    // It's acceptable for `read` to return `0` in UDP case. This means an empty
    // datagram (a UDP packet with only headers) is received.
    //
    // Don't treat this as an error.
    auto read = io::detail::EIntrSafeRecvFrom(
        fd(), buffer.data(), buffer.size(), 0 /* MSG_CMSG_CLOEXEC? */,
        er.RetrieveAddr(), er.RetrieveLength());
    if (read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return EventAction::Ready;
      } else {
        Kill(CleanupReason::Error);
        return EventAction::Leaving;
      }
    }
    CHECK_LE(read, buffer.size());

    // Call the user's handler.
    //
    // TODO(luobogao): `buffer` is copied. Optimize this.
    auto action = options_.handler->OnDatagramArrival(
        CreateBufferSlow(buffer.data(), read), er.Build());
    if (action == DatagramTransceiverHandler::DataConsumptionStatus::Consumed) {
      // NOTHING.
    } else if (action ==
               DatagramTransceiverHandler::DataConsumptionStatus::Error) {
      Kill(CleanupReason::Error);
      return EventAction::Leaving;
    } else if (action == DatagramTransceiverHandler::DataConsumptionStatus::
                             SuppressRead) {
      return EventAction::Suppress;
    }
  }
}

Descriptor::EventAction NativeDatagramTransceiver::OnWritable() {
  auto status = FlushWritingBuffer(std::numeric_limits<std::size_t>::max());

  if (status == FlushStatus::SystemBufferSaturated) {
    return EventAction::Ready;
  } else if (status == FlushStatus::Flushed) {
    options_.handler->OnPendingWritesFlushed();
    return EventAction::Suppress;
  } else if (status == FlushStatus::PartialWrite ||
             status == FlushStatus::NothingWritten ||
             status == FlushStatus::Error) {
    Kill(CleanupReason::Error);
    return EventAction::Leaving;
  } else {
    CHECK(0) << "Unexpected status from FlushWritingBuffer: "
             << static_cast<int>(status);
  }
}

void NativeDatagramTransceiver::OnError(int err) {
  FLARE_VLOG(10, "Error on datagram transceiver {}: {}", fmt::ptr(this),
             strerror(err));
  options_.handler->OnError();
}

void NativeDatagramTransceiver::OnCleanup(CleanupReason reason) {
  CHECK(reason != CleanupReason::None);
  if (reason == CleanupReason::UserInitiated ||
      reason == CleanupReason::Disconnect) {
    // NOTHING.
  } else {
    options_.handler->OnError();
  }
  options_.handler->OnDetach();
}

NativeDatagramTransceiver::FlushStatus
NativeDatagramTransceiver::FlushWritingBuffer(std::size_t max_writes) {
  bool ever_succeeded = false;
  while (max_writes--) {
    bool emptied;
    std::uintptr_t ctx;
    auto rc = write_buffer_.FlushTo(fd(), &ctx, &emptied);

    if (rc == 0) {
      return ever_succeeded ? FlushStatus::PartialWrite
                            : FlushStatus::NothingWritten;
    } else if (rc < 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        return FlushStatus::SystemBufferSaturated;
      } else {
        return FlushStatus::Error;
      }
    }

    options_.handler->OnDatagramWritten(ctx);
    if (emptied) {
      return FlushStatus::Flushed;
    }
  }
  return FlushStatus::QuotaExceeded;
}

}  // namespace flare
