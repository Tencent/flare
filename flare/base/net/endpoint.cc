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

#include "flare/base/net/endpoint.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/if_packet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "flare/base/string.h"

using namespace std::literals;

namespace flare {

namespace {

std::string SockAddrToString(const sockaddr* addr) {
  auto af = addr->sa_family;
  switch (af) {
    case AF_INET: {
      auto p = reinterpret_cast<const sockaddr_in*>(addr);
      uint32_t s_addr = ntohl(p->sin_addr.s_addr);
      return Format("{}.{}.{}.{}:{}", (s_addr >> 24) & 0xff,
                    (s_addr >> 16) & 0xff, (s_addr >> 8) & 0xff, s_addr & 0xff,
                    ntohs(p->sin_port));
    }
    case AF_INET6: {
      auto p = reinterpret_cast<const sockaddr_in6*>(addr);
      char ip[INET6_ADDRSTRLEN];
      FLARE_CHECK(inet_ntop(af, &p->sin6_addr, ip, INET6_ADDRSTRLEN));
      return Format("[{}]:{}", ip, ntohs(p->sin6_port));
    }
    case AF_UNIX: {
      auto p = reinterpret_cast<const sockaddr_un*>(addr);
      if (p->sun_path[0] == '\0' && p->sun_path[1] != '\0') {
        return "@"s + &p->sun_path[1];
      } else {
        return p->sun_path;
      }
    }
    default: {
      return Format("TODO: Endpoint::ToString() for AF #{}.", af);
    }
  }
}

}  // namespace

#define INTERNAL_PTR() (*reinterpret_cast<sockaddr_storage**>(&storage_))
#define INTERNAL_CPTR() (*reinterpret_cast<sockaddr_storage* const*>(&storage_))

sockaddr* EndpointRetriever::RetrieveAddr() {
  return reinterpret_cast<sockaddr*>(&storage_);
}

socklen_t* EndpointRetriever::RetrieveLength() { return &length_; }

Endpoint EndpointRetriever::Build() {
  return Endpoint(RetrieveAddr(), length_);
}

Endpoint::Endpoint(const sockaddr* addr, socklen_t len) : length_(len) {
  if (length_ <= kOptimizedSize) {
    memcpy(&storage_, addr, length_);
  } else {
    INTERNAL_PTR() = new sockaddr_storage;
    memcpy(INTERNAL_PTR(), addr, length_);
  }
}

void Endpoint::SlowDestroy() { delete INTERNAL_PTR(); }

void Endpoint::SlowCopyUninitialized(const Endpoint& ep) {
  length_ = ep.Length();
  INTERNAL_PTR() = new sockaddr_storage;
  memcpy(INTERNAL_PTR(), ep.Get(), length_);
}

void Endpoint::SlowCopy(const Endpoint& ep) {
  if (!IsTriviallyCopyable()) {
    SlowDestroy();
  }
  if (ep.IsTriviallyCopyable()) {
    length_ = ep.length_;
    memcpy(&storage_, &ep.storage_, length_);
  } else {
    length_ = ep.length_;
    INTERNAL_PTR() = new sockaddr_storage;
    memcpy(INTERNAL_PTR(), ep.Get(), length_);
  }
}

const sockaddr* Endpoint::SlowGet() const {
  return reinterpret_cast<const sockaddr*>(INTERNAL_CPTR());
}

std::string Endpoint::ToString() const {
  if (FLARE_UNLIKELY(Empty())) {
    // We don't want to `CHECK(0)` here. Checking if the endpoint is initialized
    // before calling `ToString()` each time is way too complicated.
    return "(null)";
  }
  return SockAddrToString(Get());
}

bool operator==(const Endpoint& left, const Endpoint& right) {
  return memcmp(left.Get(), right.Get(), left.Length()) == 0;
}

Endpoint EndpointFromIpv4(const std::string& ip, std::uint16_t port) {
  EndpointRetriever er;
  auto addr = er.RetrieveAddr();
  auto p = reinterpret_cast<sockaddr_in*>(addr);
  memset(p, 0, sizeof(sockaddr_in));
  FLARE_PCHECK(inet_pton(AF_INET, ip.c_str(), &p->sin_addr),
               "Cannot parse [{}].", ip);
  p->sin_port = htons(port);
  p->sin_family = AF_INET;
  *er.RetrieveLength() = sizeof(sockaddr_in);
  return er.Build();
}

Endpoint EndpointFromIpv6(const std::string& ip, std::uint16_t port) {
  EndpointRetriever er;
  auto addr = er.RetrieveAddr();
  auto p = reinterpret_cast<sockaddr_in6*>(addr);
  memset(p, 0, sizeof(sockaddr_in6));
  FLARE_PCHECK(inet_pton(AF_INET6, ip.c_str(), &p->sin6_addr),
               "Cannot parse [{}].", ip);
  p->sin6_port = htons(port);
  p->sin6_family = AF_INET6;
  *er.RetrieveLength() = sizeof(sockaddr_in6);
  return er.Build();
}

std::string EndpointGetIp(const Endpoint& endpoint) {
  std::string result;
  if (endpoint.Family() == AF_INET) {
    result.resize(INET_ADDRSTRLEN + 1 /* '\x00' */);
    FLARE_CHECK(inet_ntop(endpoint.Family(),
                          &endpoint.UnsafeGet<sockaddr_in>()->sin_addr,
                          result.data(), result.size()));
  } else if (endpoint.Family() == AF_INET6) {
    result.resize(INET6_ADDRSTRLEN + 1 /* '\x00' */);
    FLARE_CHECK(inet_ntop(endpoint.Family(),
                          &endpoint.UnsafeGet<sockaddr_in6>()->sin6_addr,
                          result.data(), result.size()));
  } else {
    FLARE_CHECK(
        0, "Unexpected: Address family #{} is not a valid IP address family.",
        endpoint.Family());
  }
  return result.substr(0, result.find('\x00'));
}

std::uint16_t EndpointGetPort(const Endpoint& endpoint) {
  FLARE_CHECK(
      endpoint.Family() == AF_INET || endpoint.Family() == AF_INET6,
      "Unexpected: Address family #{} is not a valid IP address family.",
      endpoint.Family());
  if (endpoint.Family() == AF_INET) {
    return ntohs(
        reinterpret_cast<const sockaddr_in*>(endpoint.Get())->sin_port);
  } else if (endpoint.Family() == AF_INET6) {
    return ntohs(
        reinterpret_cast<const sockaddr_in6*>(endpoint.Get())->sin6_port);
  }
  FLARE_UNREACHABLE();
}

Expected<std::vector<Endpoint>, int> ResolveDomain(const std::string& domain,
                                                   uint16_t port) {
  std::vector<Endpoint> ret;
  addrinfo hints, *result;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  auto port_str = std::to_string(port);
  auto service = (port != 0) ? port_str.c_str() : nullptr;

  if (int rt = getaddrinfo(domain.c_str(), service, &hints, &result); rt != 0) {
    // TODO log
    return rt;
  }
  for (auto curr = result; curr != nullptr; curr = curr->ai_next) {
    EndpointRetriever er;
    auto addr = er.RetrieveAddr();
    memcpy(addr, curr->ai_addr, curr->ai_addrlen);
    *er.RetrieveLength() = curr->ai_addrlen;
    ret.emplace_back(er.Build());
  }
  freeaddrinfo(result);
  return ret;
}

std::vector<Endpoint> GetInterfaceAddresses() {
  ifaddrs* ifs_raw;
  FLARE_PCHECK(getifaddrs(&ifs_raw) == 0, "Cannot enumerate NICs.");
  std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> ifs(ifs_raw, &freeifaddrs);

  std::vector<Endpoint> result;
  for (auto current = ifs.get(); current; current = current->ifa_next) {
    EndpointRetriever er;
    auto addr = current->ifa_addr;
    if (!addr) {
      FLARE_LOG_WARNING_ONCE(
          "Skipping device [{}] when enumerating interface addresses. No "
          "address is assigned to this device.",
          current->ifa_name);
      continue;
    }
    auto af = addr->sa_family;

#define TEST_AF_AND_COPY(AddressFamily, Storage)      \
  case AddressFamily: {                               \
    memcpy(er.RetrieveAddr(), addr, sizeof(Storage)); \
    *er.RetrieveLength() = sizeof(Storage);           \
    break;                                            \
  }

    switch (af) {
      TEST_AF_AND_COPY(AF_INET, sockaddr_in)
      TEST_AF_AND_COPY(AF_INET6, sockaddr_in6)
      TEST_AF_AND_COPY(AF_UNIX, sockaddr_un)
      case AF_PACKET: {
        continue;  // Ignored.
      }
      default: {
        FLARE_LOG_WARNING_ONCE("Unrecognized address family #{} is ignored.",
                               af);
        continue;
      }

#undef TEST_AF_AND_COPY
    }
    result.push_back(er.Build());
  }
  return result;
}

bool IsPrivateIpv4AddressRfc(const Endpoint& addr) {
  constexpr std::pair<std::uint32_t, std::uint32_t> kRanges[] = {
      {0xFF000000, 0x0A000000},  // 10.0.0.0/8
      {0xFFF00000, 0xAC100000},  // 172.16.0.0/12
      {0xFFFF0000, 0xC0A80000},  // 192.168.0.0/16
  };

  if (addr.Family() != AF_INET) {
    return false;
  }

  auto ip = ntohl(addr.UnsafeGet<sockaddr_in>()->sin_addr.s_addr);
  for (auto&& [mask, expected] : kRanges) {
    if ((ip & mask) == expected) {
      return true;
    }
  }
  return false;
}

bool IsPrivateIpv4AddressCorp(const Endpoint& addr) {
  constexpr std::pair<std::uint32_t, std::uint32_t> kRanges[] = {
      {0xFF000000, 0x0A000000},  // 10.0.0.0/8
      {0xFFC00000, 0x64400000},  // 100.64.0.0/10
      {0xFFF00000, 0xAC100000},  // 172.16.0.0/12
      {0xFFFF0000, 0xC0A80000},  // 192.168.0.0/16

      {0xFF000000, 0x09000000},  // 9.0.0.0/8
      {0xFF000000, 0x0B000000},  // 11.0.0.0/8
      {0xFF000000, 0x1E000000},  // 30.0.0.0/8
  };
  if (addr.Family() != AF_INET) {
    return false;
  }

  auto ip = ntohl(addr.UnsafeGet<sockaddr_in>()->sin_addr.s_addr);
  for (auto&& [mask, expected] : kRanges) {
    if ((ip & mask) == expected) {
      return true;
    }
  }
  return false;
}

bool IsGuaIpv6Address(const Endpoint& addr) {
  if (addr.Family() != AF_INET6) {
    return false;
  }
  auto v6 = addr.UnsafeGet<sockaddr_in6>()->sin6_addr;
  // 2000::/3
  return v6.s6_addr[0] >= 0x20 && v6.s6_addr[0] <= 0x3f;
}

std::ostream& operator<<(std::ostream& os, const Endpoint& endpoint) {
  return os << endpoint.ToString();
}

std::optional<Endpoint> TryParseTraits<Endpoint>::TryParse(std::string_view s,
                                                           from_ipv4_t) {
  auto pos = s.find(':');
  if (pos == std::string_view::npos) {
    return {};
  }
  auto ip = std::string(s.substr(0, pos));
  auto port = flare::TryParse<std::uint16_t>(s.substr(pos + 1));
  if (!port) {
    return {};
  }
  EndpointRetriever er;
  auto addr = er.RetrieveAddr();
  auto p = reinterpret_cast<sockaddr_in*>(addr);
  memset(p, 0, sizeof(sockaddr_in));
  if (inet_pton(AF_INET, ip.c_str(), &p->sin_addr) != 1) {
    return {};
  }
  p->sin_port = htons(*port);
  p->sin_family = AF_INET;
  *er.RetrieveLength() = sizeof(sockaddr_in);
  return er.Build();
}

std::optional<Endpoint> TryParseTraits<Endpoint>::TryParse(std::string_view s,
                                                           from_ipv6_t) {
  auto pos = s.find_last_of(':');
  if (pos == std::string_view::npos) {
    return {};
  }
  if (pos < 2) {
    return {};
  }
  auto ip = std::string(s.substr(1, pos - 2));
  auto port = flare::TryParse<std::uint16_t>(s.substr(pos + 1));
  if (!port) {
    return {};
  }
  EndpointRetriever er;
  auto addr = er.RetrieveAddr();
  auto p = reinterpret_cast<sockaddr_in6*>(addr);
  memset(p, 0, sizeof(sockaddr_in6));
  if (inet_pton(AF_INET6, ip.c_str(), &p->sin6_addr) != 1) {
    return {};
  }
  p->sin6_port = htons(*port);
  p->sin6_family = AF_INET6;
  *er.RetrieveLength() = sizeof(sockaddr_in6);
  return er.Build();
}

std::optional<Endpoint> TryParseTraits<Endpoint>::TryParse(std::string_view s) {
  if (auto opt = TryParse(s, from_ipv4)) {
    return opt;
  } else {
    return TryParse(s, from_ipv6);
  }
}

Endpoint EndpointFromString(const std::string& ip_port) {
  auto opt = TryParse<Endpoint>(ip_port);
  FLARE_CHECK(!!opt, "Cannot parse endpoint [{}].", ip_port);
  return *opt;
}

}  // namespace flare
