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

#include "flare/io/detail/poller.h"

#ifdef __linux__
#include "flare/io/detail/epoll_poller.h"
#endif
#ifdef __APPLE__
#include "flare/io/detail/kqueue_poller.h"
#endif

namespace flare::io::detail {

std::unique_ptr<Poller> CreatePoller() {
#if defined(__linux__)
  return std::make_unique<EpollPoller>();
#elif defined(__APPLE__)
  return std::make_unique<KqueuePoller>();
#else
#error "Unsupported platform: neither __linux__ nor __APPLE__ defined"
#endif
}

}  // namespace flare::io::detail
