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

#ifndef FLARE_IO_DETAIL_READ_AT_MOST_H_
#define FLARE_IO_DETAIL_READ_AT_MOST_H_

#include "flare/base/buffer.h"

#include "flare/io/util/stream_io.h"

namespace flare::io::detail {

enum class ReadStatus {
  // All bytes are read, the socket is left in `EAGAIN` state.
  Drained,

  // `max_bytes` are read.
  MaxBytesRead,

  // All (remaining) bytes are read, the socket is being closed by the remote
  // side.
  PeerClosing,

  // You're out of luck.
  Error
};

// Reads at most `max_bytes` into `to`.
//
// This method is more performant than issuing a call to `io->Read` for each
// buffer block.
//
// Returns whatever returned by `io->Read`.
ReadStatus ReadAtMost(std::size_t max_bytes, AbstractStreamIo* io,
                      NoncontiguousBuffer* to, std::size_t* bytes_read);

}  // namespace flare::io::detail

#endif  // FLARE_IO_DETAIL_READ_AT_MOST_H_
