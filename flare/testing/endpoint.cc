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

#include "flare/testing/endpoint.h"

#include "flare/base/handle.h"
#include "flare/base/net/endpoint.h"
#include "flare/base/random.h"

namespace flare::testing {

namespace detail {

bool IsPortAvaliable(uint16_t port, int type) {
  Handle sock{socket(PF_INET, type, 0)};
  if (sock.Get() < 0) {
    return false;
  }
  // Deliberately do NOT set SO_REUSEADDR here. This is an availability probe,
  // so it must see the port exactly as a fresh server bind would. With
  // SO_REUSEADDR the probe can bind 0.0.0.0:port even while a concurrent test's
  // server already holds 127.0.0.1:port, falsely reporting the port free and
  // causing the caller's real bind to fail with EADDRINUSE under parallel test
  // execution. Without it, an occupied port is correctly rejected and the
  // caller retries another random port.
  auto ep = EndpointFromIpv4("0.0.0.0", port);
  return bind(sock.Get(), ep.Get(), ep.Length()) == 0;
}

uint16_t PickAvailablePort(int type) {
  while (true) {
    auto port = Random(1024, 65535);
    if (IsPortAvaliable(port, type)) {
      return port;
    }
  }
}

}  // namespace detail

std::uint16_t PickAvailablePort(int type) {
  return detail::PickAvailablePort(type);
}

Endpoint PickAvailableEndpoint(int type) {
  return EndpointFromIpv4("127.0.0.1", PickAvailablePort(type));
}

}  // namespace flare::testing
