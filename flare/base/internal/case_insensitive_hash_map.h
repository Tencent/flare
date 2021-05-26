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

#ifndef FLARE_BASE_INTERNAL_CASE_INSENSITIVE_HASH_MAP_H_
#define FLARE_BASE_INTERNAL_CASE_INSENSITIVE_HASH_MAP_H_

#include <array>

#include "flare/base/internal/hash_map.h"

// DO NOT USE IT.
//
// Implement you own `CaseInsensitiveHash` & `CaseInsensitiveEqualTo` and use
// them with `HashMap` instead.
//
// This implementation as of now is still pretty low quality, we will lift it up
// into `flare::` once we're satisfied with its QoI.
//
// For internal use only.

namespace flare::internal {

namespace detail::hash_map {

static constexpr std::array<char, 256> kLowerChars = []() {
  std::array<char, 256> cs{};
  for (std::size_t index = 0; index != 256; ++index) {
    if (index >= 'A' && index <= 'Z') {
      cs[index] = index - 'A' + 'a';
    } else {
      cs[index] = index;
    }
  }
  return cs;
}();

// Both `Hash` and `EqualTo` performs badly.

// Locale is NOT taken into consideration. The implementation can be buggy in
// some locale, but for our own use it's sufficient.
constexpr char ToLower(char c) { return kLowerChars[c]; }

struct CaseInsensitiveHash {
  std::size_t operator()(const std::string_view& s) const noexcept {
    uint64_t hash = 5381;
    for (auto&& c : s) {
      hash += ((hash << 5) + hash) + ToLower(c);
    }
    return hash;
  }
};

struct CaseInsensitiveEqualTo {
  bool operator()(const std::string_view& x,
                  const std::string_view& y) const noexcept {
    if (FLARE_UNLIKELY(x.size() != y.size())) {
      return false;
    }
    for (std::size_t index = 0; index != x.size(); ++index) {
      if (ToLower(x[index]) != ToLower(y[index])) {
        return false;
      }
    }
    return true;
  }
};

}  // namespace detail::hash_map

// May be of a bit surprising, but if you used different case between operating
// the map, the first one you used is preserved as the key's case.
//
// Consider the following code:
//
// ```cpp
// CaseInsensitiveHashMap<std::string, int> m;
// m["A"] = 10;
// m["a"] = 5;
// ```
//
// When iterating the map, you will see a KV pair as `("A", 5)`. Be prepared.
template <class K, class V>
using CaseInsensitiveHashMap =
    HashMap<K, V, detail::hash_map::CaseInsensitiveHash,
            detail::hash_map::CaseInsensitiveEqualTo>;

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_CASE_INSENSITIVE_HASH_MAP_H_
