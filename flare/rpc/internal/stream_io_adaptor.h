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

#ifndef FLARE_RPC_INTERNAL_STREAM_IO_ADAPTOR_H_
#define FLARE_RPC_INTERNAL_STREAM_IO_ADAPTOR_H_

#include <atomic>
#include <memory>

#include "flare/base/function.h"
#include "flare/base/ref_ptr.h"
#include "flare/fiber/work_queue.h"
#include "flare/rpc/internal/buffered_stream_provider.h"
#include "flare/rpc/protocol/message.h"
#include "flare/rpc/internal/stream.h"

namespace flare::rpc::detail {

// This class helps you create stream reader / writer out of a series message.
//
// To serialize calls to user code (i.e., `StreamReader` / `StreamWriter` 's
// callback), it uses a `WorkQueue` internally. All calls to both your callback
// and user's code are made from the work queue.
//
// You need to wait for the work queue to stop before destroying the adaptor.
class StreamIoAdaptor {
 public:
  using MessagePtr = std::unique_ptr<Message>;

  struct Operations {
    Function<bool(MessagePtr*)> try_parse;
    Function<bool(const Message&)> write;

    // Note that `restart_read` can be called even before `NotifyRead()` is
    // returned, since, as stated in class's comments, `restart_read` is called
    // asynchronously.
    //
    // This can lead to subtle race condition: `NotifyRead` is about to return
    // `true` (indicating the data provider to suspend), but due to scheduling
    // policies, `restart_read` is called before `NotifyRead` returns. This
    // can lead to missing "restart" signal in some cases.
    //
    // Users need to take special care for this case.
    //
    // Note that, however, for `StreamConnection`, it's not an issue.
    // `StreamConnection::RestartRead` is guaranteed to work in this case. Check
    // comments there for more details.
    Function<void()> restart_read;

    // Called when both reader & writer is closed, before callback passed to
    // `StreamXxxProvider::Close` is called.
    Function<void()> on_close;

    // Called when all pending callbacks have completed.
    Function<void()> on_cleanup;
  };

  // `buffer_size` specifies maximum number of buffered messages not read by the
  // consumer (i.e., user of `GetStreamXxx()`). This is a soft limit. Once it's
  // reached, further calls to `NotifyRead()` will return false (but still
  // buffer the new message).
  StreamIoAdaptor(std::size_t buffer_size, Operations ops);

  // It's allowed to move return value away.
  //
  // The adaptor itself must be kept alive as long as the at least one of the
  // streams returned here is alive. (`Operations.on_close` is called when both
  // stream is closed.)
  AsyncStreamReader<MessagePtr>& GetStreamReader();
  AsyncStreamWriter<MessagePtr>& GetStreamWriter();

  // Returns true if internal buffer is full. The caller should suspend feeding
  // in this case.
  bool NotifyRead(MessagePtr msg);

  // Notifies the adaptor about an error (end-of-stream is treated as an error
  // here.).
  void NotifyError(StreamError error);

  // Call when writes issued by this class has completed.
  void NotifyWriteCompletion();

  // This is called when I/O media (e.g., `StreamConnection`) on which this
  // stream is running has broken.
  void Break();

  // Block until all scheduled callbacks about the streams has returned.
  void FlushPendingCalls();

 private:
  // Called upon message consumption in the input stream.
  void OnInputStreamMessageConsumption();

  // Called upon input stream being closed.
  void OnInputStreamClosed();

  // Called upon input stream being cleaned up.
  void OnInputStreamCleanup();

  // Called upon new message is written into the output stream.
  void OnOutputStreamMessageProduced(MessagePtr msg_ptr);

  // Called upon output stream being closed.
  void OnOutputStreamClosed();

  // Called upon output stream being cleaned up.
  void OnOutputStreamCleanup();

  // Called after both input stream and output stream closed.
  void OnStreamClosed();

  // Called after all pending callbacks have finished.
  void OnStreamCleanup();

  // This method help us to prevent `work_queue_` from draining. This is
  // required in certain cases where we need to take measures to prevent us from
  // being destroyed.
  //
  // Internally it posts a task into work queue to spin until the atomic bool
  // returned is set to true.
  std::atomic<bool>* PostWorkQueueBlockingTask();

 private:
  std::size_t buffer_size_;  // Maximum number of unacked reads / writes.
  Operations ops_;

  // Constructed in ctor.
  AsyncStreamReader<MessagePtr> input_stream_;
  AsyncStreamWriter<MessagePtr> output_stream_;

  // Decremented when `StreamReader` or `StreamWriter` is closed. Once both
  // reader & writer is closed, the counter reaches 0.
  std::atomic<int> remaining_users_{2};

  // We serialize all callouts in the work queue.
  fiber::WorkQueue work_queue_;

  // Number of messages we had written into `is_provider_` and yet have to be
  // read by the user.
  std::atomic<std::size_t> unacked_msgs_ = 0;

  // Number of writes (to `os_provider_`) that we have not acked.
  std::atomic<std::size_t> unacked_writes_ = 0;

  // Decremented when `StreamReader` or `StreamWriter` is cleaned up.
  int alive_streams_{2};

  // Associated with the stream defined above.
  //
  // If set to `nullptr`, the either an error has occurred on the corresponding
  // stream, or the stream was closed. Either way, we, as well as the user, must
  // treat the stream as closed, and no longer touch the it.
  RefPtr<BufferedStreamReaderProvider<MessagePtr>> is_provider_;
  RefPtr<BufferedStreamWriterProvider<MessagePtr>> os_provider_;
};

}  // namespace flare::rpc::detail

#endif  // FLARE_RPC_INTERNAL_STREAM_IO_ADAPTOR_H_
