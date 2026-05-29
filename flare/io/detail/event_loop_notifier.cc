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

#include "flare/io/detail/event_loop_notifier.h"

#include <fcntl.h>
#ifdef __linux__
#include <sys/eventfd.h>
#endif
#include <unistd.h>

#include "flare/base/logging.h"
#include "flare/io/detail/eintr_safe.h"

namespace flare::io::detail {

#ifndef __linux__
namespace {

std::pair<Handle, Handle> CreatePipePair() {
  int fds[2];
  FLARE_PCHECK(::pipe(fds) == 0, "Cannot create pipe for event loop notifier.");
  // Set both ends non-blocking and close-on-exec.
  for (int i = 0; i < 2; ++i) {
    auto flags = fcntl(fds[i], F_GETFL);
    FLARE_PCHECK(flags != -1);
    FLARE_PCHECK(fcntl(fds[i], F_SETFL, flags | O_NONBLOCK) == 0);
    auto fdflags = fcntl(fds[i], F_GETFD);
    FLARE_PCHECK(fdflags != -1);
    FLARE_PCHECK(fcntl(fds[i], F_SETFD, fdflags | FD_CLOEXEC) == 0);
  }
  return {Handle(fds[0]), Handle(fds[1])};
}

}  // namespace
#endif  // !__linux__

EventLoopNotifier::EventLoopNotifier() {
#ifdef __linux__
  fd_ = Handle(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
  FLARE_PCHECK(fd_.Get() != -1,
               "Cannot create eventfd for event loop notifier.");
#else
  auto p = CreatePipePair();
  read_fd_ = std::move(p.first);
  write_fd_ = std::move(p.second);
#endif
}

int EventLoopNotifier::fd() const noexcept {
#ifdef __linux__
  return fd_.Get();
#else
  return read_fd_.Get();
#endif
}

void EventLoopNotifier::Notify() noexcept {
#ifdef __linux__
  uint64_t u = 1;
  auto rc = io::detail::EIntrSafeWrite(fd_.Get(), &u, sizeof(u));
  PCHECK(rc == sizeof(u));
#else
  char v = 1;
  auto rc = io::detail::EIntrSafeWrite(write_fd_.Get(), &v, sizeof(v));
  PCHECK(rc == sizeof(v));
#endif
}

void EventLoopNotifier::Reset() noexcept {
#ifdef __linux__
  uint64_t u;
  int rc;
  do {
    rc = io::detail::EIntrSafeRead(fd_.Get(), &u, sizeof(u));
  } while (rc > 0);
  FLARE_CHECK(rc == -1 || rc == 0);
#else
  char v;
  int rc;
  // Keep reading until EAGAIN.
  do {
    rc = io::detail::EIntrSafeRead(read_fd_.Get(), &v, sizeof(v));
  } while (rc > 0);
  // rc == 0 means EOF (should not happen), rc == -1 means EAGAIN (expected).
  FLARE_CHECK(rc == -1 || rc == 0);
#endif
}

}  // namespace flare::io::detail
