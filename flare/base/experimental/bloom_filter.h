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

#ifndef FLARE_BASE_EXPERIMENTAL_BLOOM_FILTER_H_
#define FLARE_BASE_EXPERIMENTAL_BLOOM_FILTER_H_

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "flare/base/logging.h"
#include "flare/base/random.h"

namespace flare::experimental {

namespace bloom_filter {

namespace detail {

struct Hash;

}  // namespace detail

// Use double hash to generate a series of hash values for this key.
//
// @sa: [Kirsch, Mitzenmacher 2006]
// @sa: https://github.com/google/leveldb/blob/master/util/bloom.cc#L47
//
// Note that in our test, this generator works poorly. Unless you're perf.
// sensitive and does not really care about false positive rate, use another
// generator.
template <class H = detail::Hash>
struct DoubleHashingHashGenerator;

// Generate a series of hashes by prepending a series of "salt" to the string to
// be hashed.
//
// This generator produces better (lower) false positive rate than double
// hashing, in trade of speed.
template <class H = detail::Hash>
struct SaltedHashGenerator;

}  // namespace bloom_filter

// Implements Bloom Filter. Key is hardcoded to string (or binary bytes).
//
// **CAUTION: The default `HashGen` is NOT guaranteed to be stable across
// different version of Flare. Should you want to transfer your Bloom Filter
// across different process or different life of your program, USE YOU OWN
// GENERATOR.**
template <class HashGen = bloom_filter::SaltedHashGenerator<>>
class BloomFilter {
 public:
  // Create a Bloom Filter with `m` bits. `k` hash values are generated for each
  // key.
  BloomFilter(std::size_t m, std::size_t k);

  // Create a Bloom Filter that exhibits a false positive possibility of `p`
  // under the condition that at most `n` elements are added. `k` hash values
  // are generated for each key.
  BloomFilter(std::size_t n, double p, std::size_t k);

  // Deserialize a Bloom Filter that was serialized via `GetBytes()`.
  //
  // @sa: Caution in comments of the class.
  BloomFilter(std::string_view existing_filter, std::size_t k);

  // Parameter `k`. This parameter specifies how many iteration of double
  // hashing do we do internally to generate different hash values.
  //
  // Not sure if this is a good name though.
  std::size_t GetIterationCount() const noexcept;

  // Get internal bits. This can be used for serializing a Bloom Filter.
  //
  // @sa: Caution in comments of the class.
  std::string_view GetBytes() const noexcept;

  // Add a new key to this filter.
  void Add(std::string_view key) noexcept;

  // Test if `value` was *possibly* added into this filter.
  bool PossiblyContains(std::string_view key) const noexcept;

  // Merge another filter with this one. The new filter contains keys in both
  // filters.
  //
  // Merging two filters with different parameters (salts, size, etc) is
  // undefined.
  void MergeFrom(const BloomFilter& from) noexcept;

 private:
  // Determine number of bits needed to achieve an expected false positive
  // possibility no greater than `p`, under the condition that: 1) at most `n`
  // elements is added, and 2) exactly `k` hash functions are used.
  static std::uint64_t GetOptimalBits(double p, std::uint64_t n,
                                      std::uint64_t k);

  // Round up to nearest power of 2. This saves us a costly `idiv` in operating
  // the bits.
  static std::uint64_t GetNextPowerOfTwo(std::uint64_t value);

  // Set & get i-th bit.
  void SetBit(std::size_t at) noexcept;
  bool GetBit(std::size_t at) const noexcept;

 private:
  std::size_t num_hashes_;
  std::size_t hash_mask_;

  std::string bytes_;
};

// Hash generator of these aliases are not guaranteed to be stable across
// different version of Flare.
using SaltedBloomFilter = BloomFilter<bloom_filter::SaltedHashGenerator<>>;
using DoubleHashingBloomFilter =
    BloomFilter<bloom_filter::DoubleHashingHashGenerator<>>;

/////////////////////////////////
// Implementation goes below.  //
/////////////////////////////////

namespace bloom_filter {

namespace detail {

struct Hash {
  std::size_t operator()(std::string_view s) const noexcept;
};

}  // namespace detail

// Use double hash to generate a series of hash values for this key.
//
// @sa: [Kirsch, Mitzenmacher 2006]
// @sa: https://github.com/google/leveldb/blob/master/util/bloom.cc#L47
//
// Note that in our test, this generator works poorly. Unless you're perf.
// sensitive and does not really care about false positive rate, use another
// generator.
template <class H>
struct DoubleHashingHashGenerator {
  template <class F>
  bool operator()(std::string_view s, std::size_t n, F&& f) const noexcept {
    auto h = H()(s);
    auto delta = (h >> 17) | (h << 15);
    for (int i = 0; i != n; ++i) {
      if (!f(h)) {
        return false;
      }
      h += delta;
    }
    return true;
  }
};

// Generate a series of hashes by prepending a series of "salt" to the string to
// be hashed.
//
// This generator produces better (lower) false positive rate than double
// hashing, in trade of speed.
template <class H>
struct SaltedHashGenerator {
  using SaltInteger = int;

  template <class F>
  bool operator()(std::string_view s, std::size_t n, F&& f) const noexcept {
    char fast_buffer[128];
    std::unique_ptr<char[]> slow_buffer;

    constexpr auto kSaltSize = sizeof(SaltInteger);
    auto salted_size = s.size() + kSaltSize;
    char* buffer_ptr;

    if (salted_size >= sizeof(fast_buffer)) {
      // Don't use `std::make_unique<T[]>`, it zero-initialize the array and
      // therefore slow.
      slow_buffer.reset(new char[salted_size]);
      buffer_ptr = slow_buffer.get();
    } else {
      buffer_ptr = fast_buffer;
    }
    // GCC 8.2 raises a `-Warray-bounds` if we use a equivalent `memcpy` here,
    // possibly a BUG of GCC.
    std::copy(s.begin(), s.end(), buffer_ptr + kSaltSize);
    for (int i = 0; i != n; ++i) {
      SaltInteger salt = i;
      memcpy(buffer_ptr, &salt, sizeof(SaltInteger));
      if (!f(H()(std::string_view(buffer_ptr, salted_size)))) {
        return false;
      }
    }
    return true;
  }
};

}  // namespace bloom_filter

template <class HashGen>
BloomFilter<HashGen>::BloomFilter(std::size_t m, std::size_t k)
    : num_hashes_(k) {
  auto aligned_bits = std::max<std::size_t>(8, GetNextPowerOfTwo(m));
  hash_mask_ = aligned_bits - 1;
  bytes_.resize(aligned_bits / 8, 0);
}

template <class HashGen>
BloomFilter<HashGen>::BloomFilter(std::size_t n, double p, std::size_t k)
    : BloomFilter(GetOptimalBits(p, n, k), k) {}

template <class HashGen>
BloomFilter<HashGen>::BloomFilter(std::string_view existing_filter,
                                  std::size_t k)
    : num_hashes_(k) {
  auto bits = existing_filter.size() * 8;
  FLARE_CHECK_EQ(bits & (bits - 1), 0,
                 "Number of bits in the given Bloom Filter is not a power of "
                 "2. Importing a Bloom Filter that was not produced by us?");
  hash_mask_ = bits - 1;
  bytes_ = existing_filter;
}

template <class HashGen>
std::size_t BloomFilter<HashGen>::GetIterationCount() const noexcept {
  return num_hashes_;
}

template <class HashGen>
std::string_view BloomFilter<HashGen>::GetBytes() const noexcept {
  return bytes_;
}

template <class HashGen>
void BloomFilter<HashGen>::Add(std::string_view key) noexcept {
  HashGen()(key, num_hashes_, [&](auto h) {
    SetBit(h & hash_mask_);
    return true;
  });
}

template <class HashGen>
bool BloomFilter<HashGen>::PossiblyContains(
    std::string_view key) const noexcept {
  return HashGen()(key, num_hashes_,
                   [&](auto h) { return GetBit(h & hash_mask_); });
}

template <class HashGen>
void BloomFilter<HashGen>::MergeFrom(const BloomFilter& from) noexcept {
  FLARE_CHECK_EQ(hash_mask_, from.hash_mask_);
  FLARE_CHECK_EQ(num_hashes_, from.num_hashes_);
  FLARE_CHECK_EQ(bytes_.size(), from.bytes_.size());

  // FIXME: This can be optimized, `or`-ing bytes one by one is rather slow.
  for (std::size_t index = 0; index != from.bytes_.size(); ++index) {
    bytes_[index] |= from.bytes_[index];
  }
}

template <class HashGen>
std::uint64_t BloomFilter<HashGen>::GetOptimalBits(double p, std::uint64_t n,
                                                   std::uint64_t k) {
  // Formula at https://hur.st/bloomfilter/ (as of how `m` should be calculated)
  // seems to be wrong.
  //
  // Using formula here: https://stackoverflow.com/a/9178206
  //
  // m = 1 / (1 - (1 - p ** (1 / k)) ** (1 / (k * n)))
  auto m = 1.0 / (1.0 - pow(1.0 - pow(p, 1.0 / k), (1.0 / (1.0 * k * n))));
  return m;
}

template <class HashGen>
std::uint64_t BloomFilter<HashGen>::GetNextPowerOfTwo(std::uint64_t value) {
  return value < 2 ? value : 1ULL << (64 - __builtin_clzl(value - 1));
}

template <class HashGen>
void BloomFilter<HashGen>::SetBit(std::size_t at) noexcept {
  auto byte_pos = at / 8, bit_pos = at % 8;
  FLARE_CHECK_LT(byte_pos, bytes_.size());
  bytes_[byte_pos] |= 1 << bit_pos;
}

template <class HashGen>
bool BloomFilter<HashGen>::GetBit(std::size_t at) const noexcept {
  auto byte_pos = at / 8, bit_pos = at % 8;
  FLARE_CHECK_LT(byte_pos, bytes_.size());
  return bytes_[byte_pos] & (1 << bit_pos);
}

}  // namespace flare::experimental

#endif  // FLARE_BASE_EXPERIMENTAL_BLOOM_FILTER_H_
