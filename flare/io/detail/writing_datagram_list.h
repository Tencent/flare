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

#ifndef FLARE_IO_DETAIL_WRITING_DATAGRAM_LIST_H_
#define FLARE_IO_DETAIL_WRITING_DATAGRAM_LIST_H_

#include <deque>
#include <mutex>
#include <tuple>

#include "flare/base/buffer.h"
#include "flare/base/net/endpoint.h"

namespace flare::io::detail {

// Like `WritingBufferList`, specialized for datagrams.
class WritingDatagramList {
 public:
  // Write a datagram into `fd`.
  ssize_t FlushTo(int fd, std::uintptr_t* flushed_ctx, bool* emptied);

  // Thread-safe.
  //
  // Return true if the list is empty before. (Hence the caller is responsible
  // for starting writing.
  bool Append(Endpoint to, NoncontiguousBuffer buffer, std::uintptr_t ctx);

 private:
  // TODO(luobogao): Optimize this.
  //
  // Do NOT use fiber::Mutex here, we're using TLS internally.
  mutable std::mutex lock_;
  std::deque<std::tuple<Endpoint, NoncontiguousBuffer, std::uintptr_t>>
      buffers_;
};

}  // namespace flare::io::detail

#endif  // FLARE_IO_DETAIL_WRITING_DATAGRAM_LIST_H_
