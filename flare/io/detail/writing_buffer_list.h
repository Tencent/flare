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

#ifndef FLARE_IO_DETAIL_WRITING_BUFFER_LIST_H_
#define FLARE_IO_DETAIL_WRITING_BUFFER_LIST_H_

#include <vector>

#include "thirdparty/googletest/gtest/gtest_prod.h"

#include "flare/base/align.h"
#include "flare/base/buffer.h"
#include "flare/io/util/stream_io.h"

namespace flare {

template <class>
struct PoolTraits;

}  // namespace flare

namespace flare::io::detail {

// An MPSC writing buffer queue.
class alignas(hardware_destructive_interference_size) WritingBufferList {
 public:
  WritingBufferList();
  ~WritingBufferList();

  // Flush buffered data (via `Append`) into `fd`, up to `max_bytes` bytes.
  //
  // Due to certain implementation restrictions, return value of this method can
  // be less than `max_bytes` even when more bytes can be written.
  //
  // Normally you'd want to call this method until either `errno` is set to
  // `EAGAIN`, or `short_write` is set. The latter case means "real" short write
  // happened (if `fd` corresponds to a socket, presumably remote side has
  // closed the connection.).
  //
  // Unlike `Append`, only one thread is allowed to call this method at the same
  // time.
  //
  // Returns number of bytes written, or -1 in case of error.
  //
  //   On success (positive return value), `flushed_ctxs` returns `ctx`s
  //   associated with buffers that have been *fully* written out (@sa:
  //   `Append`). `emptied` is set if the internal buffer is emptied by this
  //   operation, otherwise it's cleared.
  ssize_t FlushTo(AbstractStreamIo* io, std::size_t max_bytes,
                  std::vector<std::uintptr_t>* flushed_ctxs, bool* emptied,
                  bool* short_write);

  // Append a buffer for writing. `ctx` is returned via `flushed_ctxs` by
  // `FlushTo` once this buffer has been written out (in its entirety).
  //
  // Thread-safe.
  //
  // Return true if the list is empty before. (Hence the caller is responsible
  // for starting writing.
  bool Append(NoncontiguousBuffer buffer, std::uintptr_t ctx);

 private:
  FRIEND_TEST(WritingBufferList, Torture);

  struct Node {
    std::atomic<Node*> next;
    NoncontiguousBuffer buffer;
    std::uintptr_t ctx;
  };
  friend struct PoolTraits<Node>;

  // @sa: [MCS locks](https://lwn.net/Articles/590243/)

  // Where we left in last `FlushTo.
  alignas(hardware_destructive_interference_size) std::atomic<Node*> head_;
  // `tail_` points to the last node.
  alignas(hardware_destructive_interference_size) std::atomic<Node*> tail_;
};

}  // namespace flare::io::detail

#endif  // FLARE_IO_DETAIL_WRITING_BUFFER_LIST_H_
