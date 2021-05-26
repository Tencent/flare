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

#include "flare/rpc/internal/stream_io_adaptor.h"

#include <atomic>
#include <utility>

#include <memory>

#include "flare/base/maybe_owning.h"
#include "flare/fiber/latch.h"
#include "flare/fiber/this_fiber.h"

namespace flare::rpc::detail {

StreamIoAdaptor::StreamIoAdaptor(std::size_t buffer_size, Operations ops)
    : buffer_size_(buffer_size), ops_(std::move(ops)) {
  CHECK_NE(buffer_size, 0);
  is_provider_ = MakeRefCounted<BufferedStreamReaderProvider<MessagePtr>>(
      buffer_size, [this] { OnInputStreamMessageConsumption(); },
      [this] { OnInputStreamClosed(); }, [this] { OnInputStreamCleanup(); });
  os_provider_ = MakeRefCounted<BufferedStreamWriterProvider<MessagePtr>>(
      buffer_size,
      [this](auto p) { OnOutputStreamMessageProduced(std::move(p)); },
      [this] { OnOutputStreamClosed(); }, [this] { OnOutputStreamCleanup(); });
  input_stream_ = AsyncStreamReader<MessagePtr>(is_provider_);
  output_stream_ = AsyncStreamWriter<MessagePtr>(os_provider_);
}

AsyncStreamReader<StreamIoAdaptor::MessagePtr>&
StreamIoAdaptor::GetStreamReader() {
  return input_stream_;
}

AsyncStreamWriter<StreamIoAdaptor::MessagePtr>&
StreamIoAdaptor::GetStreamWriter() {
  return output_stream_;
}

bool StreamIoAdaptor::NotifyRead(MessagePtr msg) {
  // This must be tested before posting jobs into `work_queue_` to avoid race.
  auto suppress =
      unacked_msgs_.fetch_add(1, std::memory_order_relaxed) >= buffer_size_ - 1;

  work_queue_.Push([this, msg = std::move(msg)]() mutable {
    // FIXME: If the stream is closed before the callback runs, we risk running
    // into use-after-free (in `ops_.try_parse`).
    if (!ops_.try_parse(&msg)) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Cannot parse message #{}. Treated as an I/O error.",
          msg->GetCorrelationId());
      is_provider_->OnDataAvailable(StreamError::IoError);
    } else {
      is_provider_->OnDataAvailable(std::move(msg));
    }
  });
  return suppress;
}

void StreamIoAdaptor::NotifyError(StreamError error) {
  unacked_msgs_.fetch_add(1, std::memory_order_relaxed);
  work_queue_.Push([this, error] { is_provider_->OnDataAvailable(error); });
}

void StreamIoAdaptor::NotifyWriteCompletion() {
  work_queue_.Push([=] { os_provider_->OnWriteCompletion(true); });
}

void StreamIoAdaptor::Break() {
  work_queue_.Push([this] {
    is_provider_->OnDataAvailable(StreamError::EndOfStream);
    os_provider_->OnWriteCompletion(false);
  });
}

void StreamIoAdaptor::FlushPendingCalls() {
  work_queue_.Stop();
  work_queue_.Join();
}

void StreamIoAdaptor::OnInputStreamMessageConsumption() {
  if (unacked_msgs_.fetch_sub(1, std::memory_order_relaxed) == buffer_size_) {
    ops_.restart_read();
  }
}

void StreamIoAdaptor::OnInputStreamClosed() {
  if (auto was = remaining_users_.fetch_sub(1, std::memory_order_relaxed);
      was == 1) {
    OnStreamClosed();
  } else {
    CHECK_EQ(was, 2);
  }
}

void StreamIoAdaptor::OnInputStreamCleanup() {
  work_queue_.Push([this] {
    if (unacked_msgs_.load(std::memory_order_relaxed) >= buffer_size_) {
      // The stream had blocked reading on this connection.
      //
      // Since the stream is being closed, the user won't have a chance
      // to consume the messages in the stream (and consequently
      // re-start reading data on the connection). So we restart reading
      // here explicitly.
      ops_.restart_read();
    }
    if (!--alive_streams_) {
      OnStreamCleanup();
    } else {
      CHECK_EQ(alive_streams_, 1);
    }
  });
}

void StreamIoAdaptor::OnOutputStreamMessageProduced(MessagePtr msg_ptr) {
  // It's possible that `ops_.write` below calls our completion callback
  // `NotifyWriteCompletion()` even before it returns. Unfortunately,
  // `NotifyWriteCompletion()` eventually leads to `OnOutputStreamClosed()`
  // being called.
  //
  // In this case, we can even be destroyed before `unacked_writes_` is
  // incremented, which is a use-after-free.
  //
  // So we block work queue from draining here. So long as work queue is not
  // drained, user would block on `FlushPendingCalls`, and won't destroy us.
  auto blocking_task = PostWorkQueueBlockingTask();

  if (ops_.write(*msg_ptr)) {
    unacked_writes_.fetch_add(1, std::memory_order_relaxed);
  } else {
    // Write failed.
    //
    // FIXME: We should fail all further write since now.
    work_queue_.Push([this] { os_provider_->OnWriteCompletion(false); });
  }
  // I do think relaxed ordering on `blocking_task` should work but TSan keep
  // reporting race (when destroying this atomic.).
  //
  // FIXME: This does not affects x86-64, but on other ISAs (e.g. AArch64), this
  // introduces unnecessary synchronization overhead.
  blocking_task->store(true, std::memory_order_release);
}

void StreamIoAdaptor::OnOutputStreamClosed() {
  if (auto was = remaining_users_.fetch_sub(1, std::memory_order_relaxed);
      was == 1) {
    OnStreamClosed();
  } else {
    CHECK_EQ(was, 2);
  }
}

void StreamIoAdaptor::OnOutputStreamCleanup() {
  work_queue_.Push([this] {
    if (!--alive_streams_) {
      OnStreamCleanup();
    } else {
      CHECK_EQ(alive_streams_, 1);
    }
  });
}

void StreamIoAdaptor::OnStreamClosed() { ops_.on_close(); }

void StreamIoAdaptor::OnStreamCleanup() { ops_.on_cleanup(); }

std::atomic<bool>* StreamIoAdaptor::PostWorkQueueBlockingTask() {
  auto ptr = std::make_unique<std::atomic<bool>>(false);
  auto rc = ptr.get();
  work_queue_.Push([ptr = std::move(ptr)] {
    while (!ptr->load(std::memory_order_acquire)) {  // The order is solely for
                                                     // comforting TSan.
      // Spin. This shouldn't take long (if we spin at all.).
      this_fiber::Yield();
    }
  });
  return rc;
}

}  // namespace flare::rpc::detail
