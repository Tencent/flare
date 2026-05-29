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

#ifndef FLARE_IO_DETAIL_KQUEUE_POLLER_H_
#define FLARE_IO_DETAIL_KQUEUE_POLLER_H_

#ifdef __APPLE__

#include <sys/event.h>
#include <vector>

#include "flare/base/handle.h"
#include "flare/io/detail/poller.h"

namespace flare::io::detail {

class KqueuePoller : public Poller {
 public:
  KqueuePoller();
  ~KqueuePoller() override = default;

  int GetFd() const override;
  void Add(int fd, int events, void* user_data) override;
  void Modify(int fd, int events, void* user_data) override;
  void Remove(int fd) override;
  int Wait(PollerEvent* events, int max_events, int timeout_ms) override;

 private:
  Handle kq_;
  std::vector<struct kevent> kevents_;
};

}  // namespace flare::io::detail

#endif  // __APPLE__

#endif  // FLARE_IO_DETAIL_KQUEUE_POLLER_H_
