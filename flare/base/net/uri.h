// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_BASE_NET_URI_H_
#define FLARE_BASE_NET_URI_H_

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "flare/base/logging.h"

namespace flare {

template <class T, class>
struct TryParseTraits;

// Represents a URI, as defined by RFC 3986.
class Uri {
 public:
  Uri() = default;

  // If `from` is malformed, the program crashes.
  //
  // To parse URI from untrusted source, use `TryParse<Uri>(...)` instead.
  explicit Uri(const std::string_view& from);

  // Accessors.
  std::string_view scheme() const noexcept { return GetComponent(kScheme); }
  std::string_view userinfo() const noexcept { return GetComponent(kUserInfo); }
  std::string_view host() const noexcept { return GetComponent(kHost); }
  std::uint16_t port() const noexcept { return port_; }
  std::string_view path() const noexcept { return GetComponent(kPath); }
  std::string_view query() const noexcept { return GetComponent(kQuery); }
  std::string_view fragment() const noexcept { return GetComponent(kFragment); }

  // Convert this object to string.
  std::string ToString() const { return uri_; }

 private:
  friend struct TryParseTraits<Uri, void>;

  // Not declared as `enum class` intentionally. We use enumerators below as
  // indices.
  enum Component {
    kScheme = 0,
    kUserInfo = 1,
    kHost = 2,
    kPort = 3,
    kPath = 4,
    kQuery = 5,
    kFragment = 6,
    kComponentCount = 7
  };

  // Using `std::uint16_t` saves memory. I don't expect a URI longer than 64K.
  using ComponentView = std::pair<std::uint16_t, std::uint16_t>;
  using Components = std::array<ComponentView, kComponentCount>;

  Uri(std::string uri, Components comps, std::uint16_t port)
      : uri_(std::move(uri)), comps_(comps), port_(port) {}

  std::string_view GetComponent(Component comp) const noexcept {
    FLARE_CHECK_NE(comp, kPort);
    FLARE_CHECK_NE(comp, kComponentCount);
    return std::string_view(uri_).substr(comps_[comp].first,
                                         comps_[comp].second);
  }

 private:
  std::string uri_;
  Components comps_;  // Into `uri_`.
  std::uint16_t port_;
};

// TODO(luobogao): `UriBuilder`.

// `std::optional<Uri> TryParse<Uri>(const std::string_view&)` is defined
// implicitly so long as `base/string.h` is included.
//
// Note that components in URI are NOT decoded as pct-encoding.

/////////////////////////////////////
// Implementation goes below.      //
/////////////////////////////////////

template <>
struct TryParseTraits<Uri, void> {
  // Should we decode pct-encoded automatically?
  //
  // static std::optional<Uri> TryParse(const std::string_view& s, bool
  // decode_pct);

  static std::optional<Uri> TryParse(const std::string_view& s);
};

}  // namespace flare

#endif  // FLARE_BASE_NET_URI_H_
