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

#ifdef __linux__

#include "flare/io/detail/epoll_poller.h"

#include <fcntl.h>

#include <vector>

#include "flare/base/logging.h"
#include "flare/io/detail/eintr_safe.h"

namespace flare::io::detail {

// EpollPoller passes these through directly as epoll_event.events.
static_assert(kPollerRead == EPOLLIN);
static_assert(kPollerWrite == EPOLLOUT);
static_assert(kPollerError == EPOLLERR);
static_assert(kPollerET == EPOLLET);

EpollPoller::EpollPoller() {
  // We use `epoll_create` here since `epoll_create1` is not available on
  // CentOS 6. `epoll_create` does not support `EPOLL_CLOEXEC` though.
  epfd_.Reset(epoll_create(1));
  FLARE_PCHECK(epfd_.Get() != -1);
  auto oldflags = fcntl(epfd_.Get(), F_GETFD);
  FLARE_PCHECK(oldflags != -1);
  FLARE_PCHECK(fcntl(epfd_.Get(), F_SETFD, oldflags | FD_CLOEXEC) == 0);
}

int EpollPoller::GetFd() const { return epfd_.Get(); }

void EpollPoller::Add(int fd, int events, void* user_data) {
  epoll_event ee;
  ee.events = events;
  ee.data.ptr = user_data;
  FLARE_PCHECK(epoll_ctl(epfd_.Get(), EPOLL_CTL_ADD, fd, &ee) == 0,
               "Failed to add fd #{} to epoll.", fd);
}

void EpollPoller::Modify(int fd, int events, void* user_data) {
  epoll_event ee;
  ee.events = events;
  ee.data.ptr = user_data;
  FLARE_PCHECK(epoll_ctl(epfd_.Get(), EPOLL_CTL_MOD, fd, &ee) == 0,
               "Failed to modify fd #{} in epoll.", fd);
}

void EpollPoller::Remove(int fd) {
  // In kernel versions before 2.6.9, the EPOLL_CTL_DEL operation required
  // a non-null pointer in event, even though this argument is ignored.
  FLARE_PCHECK(epoll_ctl(epfd_.Get(), EPOLL_CTL_DEL, fd, nullptr) == 0,
               "Failed to remove fd #{} from epoll.", fd);
}

int EpollPoller::Wait(PollerEvent* events, int max_events, int timeout_ms) {
  // FIXME: Need we use `epoll_pwait` instead to handle signal more gracefully?
  if (static_cast<int>(epoll_events_.size()) < max_events) {
    epoll_events_.resize(max_events);
  }
  auto nfds = EIntrSafeEpollWait(epfd_.Get(), epoll_events_.data(), max_events,
                                 timeout_ms);
  FLARE_PCHECK(nfds >= 0, "Unexpected: epoll_wait failed.");
  for (int i = 0; i < nfds; ++i) {
    events[i].user_data = epoll_events_[i].data.ptr;
    events[i].events = epoll_events_[i].events;
  }
  return nfds;
}

}  // namespace flare::io::detail

#endif  // __linux__
