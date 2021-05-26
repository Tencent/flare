// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/io/detail/read_at_most.h"

#include <limits.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "flare/base/internal/annotation.h"
#include "flare/base/logging.h"
#include "flare/base/object_pool.h"
#include "flare/io/util/stream_io.h"

namespace flare::io::detail {

namespace {

// Implementation-wise, if we issue `readv` with less than `UIO_FASTIOV`
// (defined as 8, as of writting) segments, the kernel eliminates a memory
// allocation (by allocating its own state on stack.).
//
// In our test, if the packets are small and there are plenty of connections,
// that memory allocation can be a significant bottleneck, so be conservative
// here.
//
// Note that, though, if the packets are large, and there are not so many
// connections, using more `iovec`s per `readv` actually boost performance.
//
// TODO(luobogao): Once variable-sized buffer blocks are implemented, we may
// want to use larger blocks on read, to improve performance in face of large
// packets.
//
// The same applies to `writev` (@sa: `writing_buffer_list.cc`). However, when
// we're writing something, we know exactly how many bytes will be written, and
// therefore won't over-allocate `iovec` array, and the contention on memory
// allocation should be not-so-great.
constexpr auto kMaxBlocksPerRead = 8;

// Refill thread-local cache of buffer blocks (if there are less than
// `kMaxBlocksPerRead` entries.) and returns a pointer to it.
std::vector<RefPtr<NativeBufferBlock>>* RefillAndGetBlocks() {
  FLARE_INTERNAL_TLS_MODEL thread_local std::vector<RefPtr<NativeBufferBlock>>
      cache;
  while (cache.size() < kMaxBlocksPerRead) {
    cache.push_back(MakeNativeBufferBlock());
  }
  return &cache;
}

// Due to technical limitations, we can only read up to `kMaxBlocksPerRead`
// blocks per call.
//
// `short_read` helps detecting for draining system buffer. This eliminates an
// unnecessary `readv`.
//
// From [http://man7.org/linux/man-pages/man7/epoll.7.html]:
//
// > For stream-oriented files (e.g., pipe, FIFO, stream socket), the condition
// > that the read/write I/O space is exhausted can also be detected by checking
// > the amount of data read from / written to the target file descriptor.  For
// > example, if you call read(2) by asking to read a certain amount of data and
// > read(2) returns a lower number of bytes, you can be sure of having
// > exhausted the read I/O space for the file descriptor.  The same is true
// > when writing using write(2).
ssize_t ReadAtMostPartial(std::size_t max_bytes, AbstractStreamIo* io,
                          NoncontiguousBuffer* to, bool* short_read) {
  auto&& block_cache = RefillAndGetBlocks();
  iovec iov[kMaxBlocksPerRead];
  FLARE_CHECK_EQ(block_cache->size(), std::size(iov));

  std::size_t iov_elements = 0;
  std::size_t bytes_to_read = 0;
  while (bytes_to_read != max_bytes && iov_elements != kMaxBlocksPerRead) {
    auto&& iove = iov[iov_elements];
    // Use blocks from back to front. This helps when we removes used blocks
    // from the cache (popping from back of a vector is cheaper.).
    auto&& block = (*block_cache)[kMaxBlocksPerRead - 1 - iov_elements];
    auto len =
        std::min(block->size(), max_bytes - bytes_to_read /* Bytes left */);

    iove.iov_base = block->mutable_data();
    iove.iov_len = len;
    bytes_to_read += len;
    ++iov_elements;
  }

  // Now perform read with `readv`.
  auto result = io->ReadV(iov, iov_elements);
  if (FLARE_UNLIKELY(result <= 0)) {
    return result;
  }
  FLARE_CHECK_LE(result, bytes_to_read);
  *short_read = result != bytes_to_read;
  std::size_t bytes_left = result;

  // Remove used blocks from the cache and move them into `to`.
  while (bytes_left) {
    auto current = std::move(block_cache->back());
    auto len = std::min(bytes_left, current->size());
    to->Append(PolymorphicBuffer(std::move(current), 0, len));
    bytes_left -= len;

    // FIXME: Even if the last block is not fully occupied, we still remove it
    // from the cache. This hurt memory utilization.
    block_cache->pop_back();
  }
  return result;
}

}  // namespace

ReadStatus ReadAtMost(std::size_t max_bytes, AbstractStreamIo* io,
                      NoncontiguousBuffer* to, std::size_t* bytes_read) {
  auto bytes_left = max_bytes;
  *bytes_read = 0;
  while (bytes_left) {
    // Read from the socket.
    bool short_read = false;  // GCC 10 reports a spurious uninitialized-var.
    auto bytes_to_read = bytes_left;
    auto read = ReadAtMostPartial(bytes_to_read, io, to, &short_read);
    if (FLARE_UNLIKELY(read == 0)) {  // The remote side closed the connection.
      return ReadStatus::PeerClosing;
    }
    if (FLARE_UNLIKELY(read < 0)) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return ReadStatus::Drained;
      } else {
        return ReadStatus::Error;
      }
    }
    FLARE_CHECK_LE(read, bytes_to_read);

    // Let's update the statistics.
    *bytes_read += read;
    bytes_left -= read;

    if (short_read) {
      FLARE_CHECK_LT(read, bytes_to_read);
      return ReadStatus::Drained;
    }
  }
  FLARE_CHECK_EQ(*bytes_read, max_bytes);
  return ReadStatus::MaxBytesRead;
}

}  // namespace flare::io::detail
