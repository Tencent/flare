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

#ifndef FLARE_IO_UTIL_SOCKET_H_
#define FLARE_IO_UTIL_SOCKET_H_

#include "flare/base/handle.h"
#include "flare/base/net/endpoint.h"

namespace flare::io::util {

// Returns a invalid handle (`(!rc == true)` holds.) on failure.

// `backlog` is capped by `net.core.somaxconn`: https://serverfault.com/q/518862
//
// If you're not able to accept connections quick enough, you're likely to lose
// them or have other troubles with accepting them.
Handle CreateListener(const Endpoint& addr, int backlog);

// For client side's use.
Handle CreateStreamSocket(sa_family_t family);
Handle CreateDatagramSocket(sa_family_t family);

bool StartConnect(int fd, const Endpoint& addr);

void SetNonBlocking(int fd);
void SetCloseOnExec(int fd);
void SetTcpNoDelay(int fd);

// Internally the kernel will double `size`.
//
// @sa: https://www.man7.org/linux/man-pages/man7/socket.7.html
void SetSendBufferSize(int fd, int size);
void SetReceiveBufferSize(int fd, int size);

int GetSocketError(int fd);

}  // namespace flare::io::util

#endif  // FLARE_IO_UTIL_SOCKET_H_
