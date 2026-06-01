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

#ifdef __APPLE__

#include "flare/io/detail/kqueue_poller.h"

#include <fcntl.h>
#include <sys/event.h>
#include <unistd.h>

#include <vector>

#include "flare/base/logging.h"
#include "flare/fiber/alternatives.h"
#include "flare/io/detail/eintr_safe.h"

namespace flare::io::detail {

namespace {

void ApplyKeventChanges(int kq, int fd, int events, void* user_data,
                        int action_flags) {
  struct kevent changes[2];
  int n = 0;

  unsigned short kflags = action_flags;
  if (events & kPollerET) {
    kflags |= EV_CLEAR;
    events &= ~kPollerET;
  }

  if (events & kPollerRead) {
    EV_SET(&changes[n++], fd, EVFILT_READ, kflags, 0, 0, user_data);
  }
  if (events & kPollerWrite) {
    EV_SET(&changes[n++], fd, EVFILT_WRITE, kflags, 0, 0, user_data);
  }

  if (n > 0) {
    int rc = kevent(kq, changes, n, nullptr, 0, nullptr);
    FLARE_PCHECK(rc != -1, "Failed to apply kqueue changes for fd #{}.", fd);
  }
}

}  // namespace

KqueuePoller::KqueuePoller() {
  kq_.Reset(kqueue());
  FLARE_PCHECK(kq_.Get() != -1);
  auto oldflags = fcntl(kq_.Get(), F_GETFD);
  FLARE_PCHECK(oldflags != -1);
  FLARE_PCHECK(fcntl(kq_.Get(), F_SETFD, oldflags | FD_CLOEXEC) == 0);
}

int KqueuePoller::GetFd() const { return kq_.Get(); }

void KqueuePoller::Add(int fd, int events, void* user_data) {
  ApplyKeventChanges(kq_.Get(), fd, events, user_data, EV_ADD | EV_ENABLE);
}

void KqueuePoller::Modify(int fd, int events, void* user_data) {
  // Unlike Linux epoll (EPOLL_CTL_MOD replaces the whole event bitmask in
  // one shot), kqueue is per-filter: EVFILT_READ and EVFILT_WRITE are two
  // independent registrations. A bare EV_ADD on one does NOT affect the
  // other -- so Modify must explicitly disable filters that are no longer
  // in the new mask, or else (under level-triggered) the kernel keeps
  // firing them, OnWritable() runs on an empty writing buffer, and the
  // connection layer mistakes the resulting NothingWritten for a peer
  // close and kills the connection.
  //
  // EV_DISABLE (not EV_DELETE) is what we want here: it pauses delivery
  // while preserving the filter's internal readiness state, mirroring
  // epoll's behaviour where EPOLL_CTL_MOD removing a flag doesn't drop
  // the readiness. (EV_DELETE + later EV_ADD on a socket that is already
  // readable produces no edge with EV_CLEAR -- the reader would stall.)
  bool et = events & kPollerET;
  unsigned short flags = EV_ADD | EV_ENABLE;
  if (et) flags |= EV_CLEAR;

  struct kevent changes[2];
  unsigned short read_flags  = (events & kPollerRead)
                                   ? flags
                                   : static_cast<unsigned short>(EV_DISABLE);
  unsigned short write_flags = (events & kPollerWrite)
                                   ? flags
                                   : static_cast<unsigned short>(EV_DISABLE);
  EV_SET(&changes[0], fd, EVFILT_READ, read_flags, 0, 0, user_data);
  EV_SET(&changes[1], fd, EVFILT_WRITE, write_flags, 0, 0, user_data);
  // EV_DISABLE on a never-registered filter returns ENOENT -- harmless.
  int rc = kevent(kq_.Get(), changes, 2, nullptr, 0, nullptr);
  if (rc == -1 && fiber::GetLastError() != ENOENT) {
    FLARE_PLOG_ERROR("Failed to modify fd #{} in kqueue.", fd);
  }
}

void KqueuePoller::Remove(int fd) {
  struct kevent changes[2];
  int n = 0;
  EV_SET(&changes[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
  EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
  // EV_DELETE on a non-existent ident/filter is harmless; ignore the return
  // value.
  int rc = kevent(kq_.Get(), changes, n, nullptr, 0, nullptr);
  if (rc == -1 && fiber::GetLastError() != ENOENT) {
    FLARE_PLOG_ERROR("Failed to remove fd #{} from kqueue.", fd);
  }
}

int KqueuePoller::Wait(PollerEvent* events, int max_events, int timeout_ms) {
  if (static_cast<int>(kevents_.size()) < max_events) {
    kevents_.resize(max_events);
  }
  struct timespec ts;
  struct timespec* pts = nullptr;
  if (timeout_ms >= 0) {
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (timeout_ms % 1000) * 1000000;
    pts = &ts;
  }
  auto nfds = EIntrSafeCall([&] {
    return kevent(kq_.Get(), nullptr, 0, kevents_.data(), max_events, pts);
  });
  FLARE_PCHECK(nfds >= 0, "Unexpected: kevent failed.");

  for (int i = 0; i < nfds; ++i) {
    events[i].user_data = kevents_[i].udata;
    // Map kqueue flags back to Poller event flags.
    int flags = 0;
    if (kevents_[i].flags & EV_ERROR) {
      flags |= kPollerError;
    } else {
      // Only set the event direction that the firing filter actually
      // represents. In particular, EV_EOF on EVFILT_READ just means the
      // peer half-closed for reading -- our write direction is still
      // usable. The upper layer detects EOF via read()/write() returning
      // 0 or EPIPE naturally; manufacturing a kPollerWrite event here
      // would cause Descriptor::FireWriteEvent to run OnWritable() with
      // an empty writing buffer, which the connection layer then mistakes
      // for "remote closed" and kills the connection.
      if (kevents_[i].filter == EVFILT_READ) {
        flags |= kPollerRead;
      }
      if (kevents_[i].filter == EVFILT_WRITE) {
        flags |= kPollerWrite;
      }
    }
    events[i].events = flags;
  }
  return nfds;
}

}  // namespace flare::io::detail

#endif  // __APPLE__
