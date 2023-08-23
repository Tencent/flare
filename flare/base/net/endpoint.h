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

#ifndef FLARE_BASE_NET_ENDPOINT_H_
#define FLARE_BASE_NET_ENDPOINT_H_

#include <netinet/in.h>
#include <sys/socket.h>

#include <cstring>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "flare/base/expected.h"
#include "flare/base/likely.h"
#include "flare/base/logging.h"

namespace flare {

class Endpoint;

// This class helps you retrieve address from syscalls. After an address is
// successfully retrieved, an `Endpoint` should be used to pass the address
// around instead.
class EndpointRetriever {
 public:
  // Usage:
  //
  // EndpointRetriever retriever;
  // accept(fd, retriever.RetrieveAddr(), retriever.RetrieveLength());
  // auto ep = retriever.ToEndpoint();  // Only call this if `accept` succeeds.
  sockaddr* RetrieveAddr();
  socklen_t* RetrieveLength();

  // Note that you may call `ToEndpoint()` only after a successful address
  // retrieval.
  Endpoint Build();

 private:
  sockaddr_storage storage_;
  socklen_t length_ = sizeof(sockaddr_storage);
};

// A substitution of gdt::SocketAddress. This one has a cleaner interface (one
// class, compared to several SocketAddressXxx), and is more space efficient
// when handling IPv4 / IPv6. (We do incur a performance penalty when dealing
// with `AF_UNIX`, though.)
//
// The class is immutable. To build an `Endpoint`, use `EndpointRetriever`.
class alignas(alignof(sockaddr_storage)) Endpoint {
 public:
  Endpoint() : length_(0) {}
  ~Endpoint();

  Endpoint(const Endpoint& ep);
  Endpoint& operator=(const Endpoint& ep);
  Endpoint(Endpoint&& ep) noexcept;
  Endpoint& operator=(Endpoint&& ep) noexcept;

  // Test if this is a default initialized (empty) one.
  //
  // FIXME: Use `operator bool()` instead? I don't think it's appropriate.
  bool Empty() const noexcept { return length_ == 0; }

  // Methods below are only applicable if `Empty()` does not hold.

  // Get the socket address stored in this object.
  const sockaddr* Get() const noexcept {
    if (FLARE_LIKELY(IsTriviallyCopyable())) {
      return reinterpret_cast<const sockaddr*>(&storage_);
    }
    return SlowGet();
  }

  // Shorthand for `reinterpret_cast<const T*>(Get())`.
  template <class T>
  const T* UnsafeGet() const noexcept {
    return reinterpret_cast<const T*>(Get());
  }

  // Get length of socket address stored in this object.
  socklen_t Length() const noexcept { return length_; }

  sa_family_t Family() const noexcept { return Get()->sa_family; }

  // Convert the endpoint to a printable string.
  std::string ToString() const;

 private:
  friend class EndpointRetriever;

  // Solely for `EndpointRetriever`'s use.
  Endpoint(const sockaddr* addr, socklen_t len);

  bool IsTriviallyCopyable() const { return length_ <= kOptimizedSize; }

  // Slow path.
  void SlowDestroy();
  void SlowCopyUninitialized(const Endpoint& ep);
  void SlowCopy(const Endpoint& ep);
  const sockaddr* SlowGet() const;

 private:
  // For address smaller than `kOptimizedSize`, they're allocated inplace.
  static constexpr auto kOptimizedSize = sizeof(sockaddr_in6);

  // CAUTION: `Storage` MUST be the first element of `Endpoint`. We ignore the
  // alignment of `sockaddr_storage` here so as to keep the entire `Endpoint` a
  // bit smaller. To keep the desired alignment, we align the `Endpoint` to be
  // that alignment and place `storage_` as the first element.
  using Storage = std::aligned_storage_t<kOptimizedSize, 1>;

  Storage storage_;
  // If `length_` is smaller than `kOptimized,` socket address is stored in
  // `storage_`, otherwise a pointer to the address is stored in `storage_`.
  socklen_t length_;
};

static_assert(sizeof(Endpoint) == 32);

bool operator==(const Endpoint& left, const Endpoint& right);

Endpoint EndpointFromIpv4(const std::string& ip, std::uint16_t port);
Endpoint EndpointFromIpv6(const std::string& ip, std::uint16_t port);
Endpoint EndpointFromUnix(std::string_view path);

// Stringify IP (no port) part of `endpoint`. It's your responsibility to make
// sure `endpoint` is indeed representing an IP address (whether it's a v4 IP or
// v6 IP.).
//
// For IPv6, the return value is NOT surrounded by '[]' (e.g: "2001:db8::1").
std::string EndpointGetIp(const Endpoint& endpoint);

// Get port part of `endpoint`.
std::uint16_t EndpointGetPort(const Endpoint& endpoint);

// Enumerate all addresses (regardless of their family) attached to this host.
std::vector<Endpoint> GetInterfaceAddresses();

// Resolve the domain name, if successful return vector<Endpoint>, otherwise
// return error_code.
// Example:
// if (auto eps = ResolveDomain("www.example.com"); eps) {
//    for (Endpoint& ep : eps.value()) {
//        // do something
//    }
// } else {
//    int error_code = eps.error();
//    // do something
// }
Expected<std::vector<Endpoint>, int> ResolveDomain(const std::string& domain,
                                                   std::uint16_t port = 0);

// For all special purposed IP address blocks, see RFC 6890.

// Test if `addr` holds an IPv4 address, and if so, if it's a private address as
// defined by RFC1918.
bool IsPrivateIpv4AddressRfc(const Endpoint& addr);

// Same as `IsPrivateIpv4AddressRfc` except that the following addresses are
// also considered private (they're used as private addresses in our corp.):
//
// - 100.64.0.0/10 (Carrier-grade NAT address)
//
// - 9.0.0.0/8
// - 11.0.0.0/8
// - 30.0.0.0/8
bool IsPrivateIpv4AddressCorp(const Endpoint& addr);

// Tests if the given address is an IPv6 address, and if so, if it's a GUA
// address.
bool IsGuaIpv6Address(const Endpoint& addr);

// Parse IP address from string. Defined implicitly so long as `base/string.h`
// is included.
//
// std::optional<Endpoint> TryParse<Endpoint>(s [, from_ipv4 | from_ipv6]);

inline constexpr struct from_ipv4_t {
  constexpr explicit from_ipv4_t() = default;
} from_ipv4;
inline constexpr struct from_ipv6_t {
  constexpr explicit from_ipv6_t() = default;
} from_ipv6;

inline constexpr struct from_unix_t {
  constexpr explicit from_unix_t() = default;
} from_unix;

// Same as `os << endpoint.ToString()`.
//
// This overload also enables `Format("{}", endpoint)`.
std::ostream& operator<<(std::ostream& os, const Endpoint& endpoint);

////////////////////////////////////////
// Implementation goes below.         //
////////////////////////////////////////

inline Endpoint::~Endpoint() {
  if (FLARE_LIKELY(IsTriviallyCopyable())) {
    return;  // Nothing to do.
  }
  SlowDestroy();
}

inline Endpoint::Endpoint(const Endpoint& ep) {
  if (FLARE_LIKELY(ep.IsTriviallyCopyable())) {
    memcpy(reinterpret_cast<void*>(this), reinterpret_cast<const void*>(&ep),
           sizeof(Endpoint));
  } else {
    SlowCopyUninitialized(ep);
  }
}

inline Endpoint& Endpoint::operator=(const Endpoint& ep) {
  if (FLARE_LIKELY(IsTriviallyCopyable() && ep.IsTriviallyCopyable())) {
    memcpy(reinterpret_cast<void*>(this), reinterpret_cast<const void*>(&ep),
           sizeof(Endpoint));
  } else {
    SlowCopy(ep);
  }
  return *this;
}

inline Endpoint::Endpoint(Endpoint&& ep) noexcept {
  if (FLARE_LIKELY(ep.IsTriviallyCopyable())) {
    memcpy(reinterpret_cast<void*>(this), reinterpret_cast<const void*>(&ep),
           sizeof(Endpoint));
  } else {
    SlowCopyUninitialized(ep);
  }
}

inline Endpoint& Endpoint::operator=(Endpoint&& ep) noexcept {
  if (FLARE_LIKELY(IsTriviallyCopyable() && ep.IsTriviallyCopyable())) {
    memcpy(reinterpret_cast<void*>(this), reinterpret_cast<const void*>(&ep),
           sizeof(Endpoint));
  } else {
    SlowCopy(ep);
  }
  return *this;
}

template <class T, class>
struct TryParseTraits;

template <>
struct TryParseTraits<Endpoint, void> {
  static std::optional<Endpoint> TryParse(std::string_view s, from_ipv4_t);
  static std::optional<Endpoint> TryParse(std::string_view s, from_ipv6_t);
  static std::optional<Endpoint> TryParse(std::string_view s, from_unix_t);
  static std::optional<Endpoint> TryParse(std::string_view s);
};

// DEPRECATED. Use `flare::TryParse<Endpoint>` instead.
Endpoint EndpointFromString(const std::string& ip_port);

}  // namespace flare

#endif  // FLARE_BASE_NET_ENDPOINT_H_
