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

#include "flare/io/util/socket.h"

#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <fstream>

#include "fmt/format.h"

#include "flare/base/logging.h"
#include "flare/fiber/alternatives.h"

namespace flare::io::util {

namespace {

int MaximumBacklog() {
  std::ifstream ifs("/proc/sys/net/core/somaxconn");
  int rc;
  ifs >> rc;
  if (!ifs) {
    return -1;
  }
  return rc;
}

Handle Socket(int af, int type, int protocol) {
  Handle fd(socket(af, type, protocol));
  if (!fd) {
    FLARE_PLOG_WARNING("Calling socket({}, {}, {}) failed.", af, type,
                       protocol);
  }
  return fd;
}

template <class T>
bool SetSockOpt(int fd, int level, int opt, T value) {
  if (setsockopt(fd, level, opt, &value, sizeof(value)) == -1) {
    FLARE_PLOG_WARNING("Cannot set option #{} on fd #{}.", opt, fd);
    return false;
  }
  return true;
}

template <class T>
bool GetSockOpt(int fd, int level, int opt, T* value) {
  socklen_t len = sizeof(T);
  if (getsockopt(fd, level, opt, value, &len) == -1) {
    FLARE_PLOG_WARNING("Cannot get option #{} from fd #{}.", opt, fd);
    return false;
  }
  CHECK_EQ(len, sizeof(T));
  return true;
}

// fd_flags |= flags
//
// Old flags is returned.
int SetFlags(int fd, int flags) {
  int old = fcntl(fd, F_GETFL, 0);
  FLARE_PCHECK(old != -1, "Cannot get fd #{}'s flags.", fd);
  int newf = old | flags;
  FLARE_PCHECK(fcntl(fd, F_SETFL, newf) == 0,
               "Cannot set fd #{}'s flags to {}.", fd, newf);
  return old;
}

}  // namespace

Handle CreateListener(const Endpoint& addr, int backlog) {
  // For performance reasons, we don't expect this value to change (even if it
  // can.)
  static const int kMaximumBacklog = [] {
    auto rc = MaximumBacklog();
    if (rc == -1) {
      FLARE_LOG_WARNING_ONCE(
          "CreateListener: Failed to read from `/proc/sys/net/core/somaxconn`. "
          "The program will keep functioning, but errors in `backlog` "
          "specified in calling `CreateListener` won't be detected.");
    }
    return rc;
  }();

  // Check if the `backlog` is capped by `net.core.maxsoconn`.
  if (kMaximumBacklog != -1 && kMaximumBacklog < backlog) {
    FLARE_LOG_WARNING_ONCE(
        "CreateListener: `backlog` you specified ({}) is larger than "
        "`net.core.maxsoconn` ({}). The latter will be the effective one. This "
        "may lead to unexpected connection failures. Consider changing "
        "`/proc/sys/net/core/somaxconn` if you indeed want such a large "
        "`backlog`.",
        backlog, kMaximumBacklog);
  }

  // Create the socket and listen on `addr`.
  auto family = addr.Get()->sa_family;
  CHECK(family != AF_UNSPEC) << "Address family is not specified.";
  CHECK(family == AF_INET || family == AF_INET6 || family == AF_UNIX)
      << "Unsupported address family: " << family;
  auto rc = Socket(addr.Get()->sa_family,
                   SOCK_STREAM,  // Datagram listener is not supported yet.
                   0);
  if (!rc) {
    return {};
  }
  if (!SetSockOpt<int>(rc.Get(), SOL_SOCKET, SO_REUSEADDR, 1)) {
    return {};
  }
  if (bind(rc.Get(), addr.Get(), addr.Length()) != 0) {
    FLARE_PLOG_WARNING("Cannot bind socket to [{}]. ", addr.ToString());
    return {};
  }
  if (listen(rc.Get(), backlog) != 0) {
    FLARE_PLOG_WARNING("Cannot listen on [{}]. ", addr.ToString());
    return {};
  }
  return rc;
}

Handle CreateStreamSocket(sa_family_t family) {
  return Socket(family, SOCK_STREAM, 0);
}

Handle CreateDatagramSocket(sa_family_t family) {
  return Socket(family, SOCK_DGRAM, 0);
}

bool StartConnect(int fd, const Endpoint& addr) {
  if (connect(fd, addr.Get(), addr.Length()) == -1 &&
      fiber::GetLastError() != EINPROGRESS) {
    FLARE_PLOG_WARNING("Cannot connect fd #{} to {}", fd, addr.ToString());
    return false;
  }
  return true;
}

void SetNonBlocking(int fd) { SetFlags(fd, O_NONBLOCK); }

void SetCloseOnExec(int fd) { SetFlags(fd, FD_CLOEXEC); }

void SetTcpNoDelay(int fd) {
  FLARE_PCHECK(SetSockOpt(fd, IPPROTO_TCP, TCP_NODELAY, 1),
               "Failed to set TCP_NODELAY on socket [{}].", fd);
}

void SetSendBufferSize(int fd, int size) {
  FLARE_PCHECK(SetSockOpt(fd, SOL_SOCKET, SO_SNDBUF, size),
               "Failed to set socket send buffer size to [{}] on socket [{}].",
               size, fd);
}

void SetReceiveBufferSize(int fd, int size) {
  FLARE_PCHECK(
      SetSockOpt(fd, SOL_SOCKET, SO_RCVBUF, size),
      "Failed to set socket receive buffer size to [{}] on socket [{}].", size,
      fd);
}

int GetSocketError(int fd) {
  int err;
  CHECK(GetSockOpt(fd, SOL_SOCKET, SO_ERROR, &err));
  return err;
}

}  // namespace flare::io::util
