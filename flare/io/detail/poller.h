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

#ifndef FLARE_IO_DETAIL_POLLER_H_
#define FLARE_IO_DETAIL_POLLER_H_

#include <memory>

namespace flare::io::detail {

// Event flag constants. Values intentionally match EPOLLIN/EPOLLOUT/EPOLLERR
// and EPOLLET so that existing Descriptor code treating them as bitmask values
// continues to work.
constexpr int kPollerRead = 0x001;   // EPOLLIN
constexpr int kPollerWrite = 0x004;  // EPOLLOUT
constexpr int kPollerError = 0x008;  // EPOLLERR
constexpr int kPollerET = 1u << 31;  // EPOLLET

// Platform-neutral event returned by Poller::Wait.
struct PollerEvent {
  void* user_data;
  int events;
};

// Abstract poller interface. Each platform provides its own implementation
// (epoll on Linux, kqueue on macOS/BSD).
class Poller {
 public:
  virtual ~Poller() = default;

  // Returns the underlying fd (epoll fd / kqueue fd) for Poller implementations
  // that use a file descriptor. Used for adding the internal notifier.
  virtual int GetFd() const = 0;

  // Add `fd` to the interest list, monitoring `events` (kPollerRead | ...).
  // `user_data` is passed back via PollerEvent::user_data on Wait().
  virtual void Add(int fd, int events, void* user_data) = 0;

  // Update the events and/or user_data for a previously-added `fd`.
  virtual void Modify(int fd, int events, void* user_data) = 0;

  // Remove `fd` from the interest list.
  virtual void Remove(int fd) = 0;

  // Wait for events. Returns the number of ready events, or -1 on error.
  virtual int Wait(PollerEvent* events, int max_events, int timeout_ms) = 0;
};

// Factory that creates the appropriate Poller for the current platform.
std::unique_ptr<Poller> CreatePoller();

}  // namespace flare::io::detail

#endif  // FLARE_IO_DETAIL_POLLER_H_
