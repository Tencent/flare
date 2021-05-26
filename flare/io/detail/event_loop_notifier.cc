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
#include <sys/eventfd.h>
#include <unistd.h>

#include "flare/base/logging.h"
#include "flare/io/detail/eintr_safe.h"

namespace flare::io::detail {

namespace {

Handle CreateEvent() {
  Handle fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK));
  FLARE_PCHECK(fd, "Cannot create eventfd.");
  return fd;
}

}  // namespace

EventLoopNotifier::EventLoopNotifier() : fd_(CreateEvent()) {}

int EventLoopNotifier::fd() const noexcept { return fd_.Get(); }

void EventLoopNotifier::Notify() noexcept {
  std::uint64_t v = 1;
  PCHECK(io::detail::EIntrSafeWrite(fd(), &v, sizeof(v)) == sizeof(v));
}

void EventLoopNotifier::Reset() noexcept {
  std::uint64_t v;
  int rc;

  // Keep reading until EOF is met.
  //
  // This shouldn't take too long, as there should be only
  // number-of-Notify-calls bytes readable anyway.
  do {
    rc = io::detail::EIntrSafeRead(fd(), &v, sizeof(v));
    PCHECK(rc >= 0 || errno == EAGAIN || errno == EWOULDBLOCK);
  } while (rc > 0);
}

}  // namespace flare::io::detail
