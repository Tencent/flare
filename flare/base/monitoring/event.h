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

#ifndef FLARE_BASE_MONITORING_EVENT_H_
#define FLARE_BASE_MONITORING_EVENT_H_

#include <array>
#include <chrono>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "flare/base/experimental/flyweight.h"
#include "flare/base/internal/hash_map.h"
#include "flare/base/likely.h"
#include "flare/base/logging.h"
#include "flare/base/monitoring/fwd.h"

namespace flare::monitoring {

namespace detail {

struct TagArray {
  std::vector<std::pair<std::string, std::string>> value;
};

}  // namespace detail

// This class helps us in looking up tags in a `HashMap`.
//
// Perf. note: Constructing it is slow.
class ComparableTags {
 public:
  explicit ComparableTags(
      std::vector<std::pair<std::string, std::string>> tags);

  const auto& GetTags() const noexcept { return tags_->value; }

  // Compares if the two tags are equivalent.
  //
  // FIXME: The implementation is relatively slow..
  bool operator==(
      const std::initializer_list<
          std::pair<std::string_view, std::string_view>>& other) const noexcept;

  bool operator==(const ComparableTags& other) const noexcept {
    return tags_ == other.tags_;
  }

 private:
  experimental::Flyweight<detail::TagArray> tags_;
};

struct Event {
  // libstdc++'s `std::variant` wastes 8 bytes (x86-64, presumably a
  // `std::size_t`) for storing active object index, that's horrible. I was
  // expecting a `std::uint8_t`, which should be far more than enough in most
  // cases, to be used instead.
  //
  // We'd better roll our own `std::variant` then. 31 bytes of char + 1 byte of
  // length (served both as a hint of which element is active, and, in case
  // we're really storing a char array, reflects the string length.) should
  // serve us well. (We might as well move this optimized string out as a
  // standalone utility class for general use.)
  using CharArray = std::array<char, 24>;

  Event() = default;
  Event(Reading expected_reading, const std::string_view& sv,
        std::uint64_t value,
        std::initializer_list<std::pair<std::string_view, std::string_view>>
            tags);

  // Interprets `key` in the right way.
  std::string_view GetKey() const noexcept;

  Reading expected_reading;
  // Here is an optimization. Copying into `CharArray` is *really* fast (esp.
  // when compared to copying a COW-ed `std::string`). Given that key names are
  // likely to be short, we expect in most cases `CharArray` is used.
  //
  // Frankly we're just working around libstdc++'s COW-ed implementation. We
  // don't have to bother doing this if we can use GCC5's new ABI or Clang's
  // libc++ (both of them have already done SSO internally.).
  std::variant<CharArray, std::string> key;
  std::uint64_t value;
  std::vector<std::pair<std::string, std::string>> tags;
  // Timestamp?
};

struct CoalescedCounterEvent {  // Or `CoalescedCounterEvents` (plural)?
  // `key` is always copied to here. Given that the reports are merged before
  // reporting to monitoring system, this shouldn't be done too often anyway.
  std::string key;

  // Tags carried with this report.
  //
  // The framework guarantees that it only coalesces reports with the same tags
  // together.
  std::vector<std::pair<std::string, std::string>> tags;

  // Total sum of values reported.
  std::uint64_t sum;

  // Number of times the counter was added.
  std::uint64_t times;
};

// Same as `MergedCounter`, at least for now.
struct CoalescedGaugeEvent {
  std::string key;
  std::vector<std::pair<std::string, std::string>> tags;
  std::int64_t sum;
  std::uint64_t times;
};

struct CoalescedTimerEvent {
  std::string key;
  std::vector<std::pair<std::string, std::string>> tags;
  std::chrono::nanoseconds unit;
  std::vector<std::pair<std::chrono::nanoseconds, std::size_t>> times;
};

inline Event::Event(
    Reading expected_reading, const std::string_view& sv, std::uint64_t value,
    std::initializer_list<std::pair<std::string_view, std::string_view>> tags)
    : expected_reading(expected_reading), value(value) {
  // static_assert(sizeof(this->key) == 32);  // Not applicable on non-x86.

  if (auto&& k = std::get<0>(this->key); FLARE_LIKELY(sv.size() < k.size())) {
    memcpy(k.data(), sv.data(), sv.size());
    k.data()[sv.size()] = 0;  // Make it null-terminated (otherwise we'd lost
                              // length information.).
  } else {
    this->key.emplace<1>(sv);
  }
  if (FLARE_UNLIKELY(tags.size() != 0)) {  // Explicitly checking for empty
                                           // saves some instructions.
    for (auto&& [k, v] : tags) {
      this->tags.emplace_back(std::string(k), std::string(v));
    }
  }
}

inline std::string_view Event::GetKey() const noexcept {
  if (FLARE_LIKELY(key.index() == 0)) {
    return std::get<0>(key).data();
  } else {
    return std::get<1>(key);
  }
}

}  // namespace flare::monitoring

#endif  // FLARE_BASE_MONITORING_EVENT_H_
