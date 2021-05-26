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

#include "flare/io/detail/eintr_safe.h"

namespace flare::io::detail {

int EIntrSafeAccept(int sockfd, sockaddr* addr, socklen_t* addrlen) {
  return EIntrSafeCall([&] { return accept(sockfd, addr, addrlen); });
}

int EIntrSafeEpollWait(int epfd, epoll_event* events, int maxevents,
                       int timeout) {
  return EIntrSafeCall(
      [&] { return epoll_wait(epfd, events, maxevents, timeout); });
}

ssize_t EIntrSafeRecvFrom(int sockfd, void* buf, size_t len, int flags,
                          sockaddr* src_addr, socklen_t* addrlen) {
  return EIntrSafeCall(
      [&] { return recvfrom(sockfd, buf, len, flags, src_addr, addrlen); });
}

ssize_t EIntrSafeSendTo(int sockfd, const void* buf, size_t len, int flags,
                        const sockaddr* dest_addr, socklen_t addrlen) {
  return EIntrSafeCall(
      [&] { return sendto(sockfd, buf, len, flags, dest_addr, addrlen); });
}

ssize_t EIntrSafeSendMsg(int sockfd, const msghdr* msg, int flags) {
  return EIntrSafeCall([&] { return sendmsg(sockfd, msg, flags); });
}

}  // namespace flare::io::detail
