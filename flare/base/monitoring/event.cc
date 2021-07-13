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

#include "flare/base/monitoring/event.h"

#include <algorithm>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace flare::experimental {

struct TagArrayHash {
  std::size_t operator()(
      const monitoring::detail::TagArray& array) const noexcept {
    std::size_t value = 0;
    for (auto&& [k, v] : array.value) {
      value = value * 131313131 + std::hash<std::string_view>()(k) * 13131 +
              std::hash<std::string_view>()(v);
    }
    return value;
  }
};

struct TagArrayEqual {
  bool operator()(const monitoring::detail::TagArray& left,
                  const monitoring::detail::TagArray& right) const noexcept {
    return left.value == right.value;
  }
};

template <>
struct FlyweightTraits<monitoring::detail::TagArray> {
  using Repository =
      experimental::flyweight::DefaultRepository<monitoring::detail::TagArray,
                                                 TagArrayHash, TagArrayEqual>;
};

}  // namespace flare::experimental

namespace flare::monitoring {

ComparableTags::ComparableTags(
    std::vector<std::pair<std::string, std::string>> tags)
    : tags_(experimental::MakeFlyweight<detail::TagArray>(
          detail::TagArray{std::move(tags)})) {}

bool ComparableTags::operator==(
    std::initializer_list<std::pair<std::string_view, std::string_view>> other)
    const noexcept {
  if (tags_->value.size() != other.size()) {
    return false;
  }
  auto iter = tags_->value.begin();
  auto iter2 = other.begin();
  while (iter != tags_->value.end()) {
    if (iter->first != iter2->first || iter->second != iter2->second) {
      return false;
    }
    ++iter;
    ++iter2;
  }
  return true;
}

}  // namespace flare::monitoring
