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

#ifndef FLARE_IO_DETAIL_EINTR_SAFE_H_
#define FLARE_IO_DETAIL_EINTR_SAFE_H_

#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <utility>

#include "flare/base/likely.h"
#include "flare/fiber/alternatives.h"

namespace flare::io::detail {

// Keep calling a method until `errno` is not `EINTR`.
//
// In most cases you should be calling the wrappers of syscall below. But in
// case the wrappers does not satisfy your need, this method could be handy.
//
// `f` is required to return non-negative on success, in which case `errno` is
// not checked.

// Slow version, won't be inlined.
template <class F>
[[gnu::noinline]] auto EIntrSafeCallSlow(F&& f) {
  // Slow path.
  while (true) {
    auto rc = std::forward<F>(f)();
    if (FLARE_LIKELY(rc >= 0 || fiber::GetLastError() != EINTR)) {
      return rc;
    }
  }
}

// Fast path, likely to be inlined.
template <class F>
inline auto EIntrSafeCall(F&& f) {
  auto rc = std::forward<F>(f)();
  if (FLARE_LIKELY(rc >= 0 || fiber::GetLastError() != EINTR)) {
    return rc;
  }

  return EIntrSafeCallSlow(std::forward<F>(f));
}

// `size_t` is not qualified with `std::` intentionally. We try to keep
// consistent with syscall's signature as much as possible.

// These methods are used frequently, so we make them inline-able here.
inline ssize_t EIntrSafeRead(int fd, void* buf, size_t count) {
  return EIntrSafeCall([&] { return read(fd, buf, count); });
}

inline ssize_t EIntrSafeWrite(int fd, const void* buf, size_t count) {
  return EIntrSafeCall([&] { return write(fd, buf, count); });
}

inline ssize_t EIntrSafeReadV(int fd, const iovec* iov, int iovcnt) {
  // FIXME: Does `readv` really return `EINTR`?
  return EIntrSafeCall([&] { return readv(fd, iov, iovcnt); });
}

inline ssize_t EIntrSafeWriteV(int fd, const iovec* iov, int iovcnt) {
  // FIXME: Does `writev` really return `EINTR`?
  return EIntrSafeCall([&] { return writev(fd, iov, iovcnt); });
}

int EIntrSafeAccept(int sockfd, sockaddr* addr, socklen_t* addrlen);
int EIntrSafeEpollWait(int epfd, epoll_event* events, int maxevents,
                       int timeout);
ssize_t EIntrSafeRecvFrom(int sockfd, void* buf, size_t len, int flags,
                          sockaddr* src_addr, socklen_t* addrlen);
ssize_t EIntrSafeSendTo(int sockfd, const void* buf, size_t len, int flags,
                        const sockaddr* dest_addr, socklen_t addrlen);
ssize_t EIntrSafeSendMsg(int sockfd, const struct msghdr* msg, int flags);

}  // namespace flare::io::detail

#endif  // FLARE_IO_DETAIL_EINTR_SAFE_H_
