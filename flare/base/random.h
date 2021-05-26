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
//
// General purpose random number generating methods. They are drop-in
// replacements for `rand` / `rand_r`.
//
// CAUTION: NOTHING HERE SHOULD BE CONSIDERED CRYPTOGRAPHICALLY SECURE. FOR
// CRYPTOGRAPHIC PURPOSE, USE OpenSSL INSTEAD.

#ifndef FLARE_BASE_RANDOM_H_
#define FLARE_BASE_RANDOM_H_

#include <cstdint>
#include <random>
#include <type_traits>

#include "flare/base/internal/annotation.h"

namespace flare {

namespace random {

// Thread local random engine.
//
// `std::mt19937_64` performs equally well as `std::mt19937` on x86_64. So we
// don't bother using `std::mt19937` for smaller types.
//
// We export this method into `gdt::random` (instead of keeping it in some
// private namespace) so that people who randomly need an engine could directly
// use this one, rather than declaring their own.
inline std::mt19937_64& Engine() {
  // This could be quite tricky.
  //
  // Ideally we'd like to initialize the PRNG engine's internal state with raw
  // bits from `std::random_device`. However, per spec, the engine only accepts
  // a single number, or a `SeedSequence`, which is not satisfied by
  // `std::random-device`, in its constructor. In the meantime, by using
  // `std::seed_seq`, we potentially degrade our randomness.
  //
  // It would be the best if the Standard requires `std::random_device` to
  // satisfy `SeedSequence`, or (better yet), allows `std::mt19937` to accept a
  // `std::random_device`.
  //
  // More detailed discussion about the initialization of PRNG engine could be
  // found at:
  //
  //   - https://stackoverflow.com/questions/15509270
  //   - http://www.pcg-random.org/posts/cpp-seeding-surprises.html
  //   - https://www.reddit.com/r/cpp/comments/31857s
  //
  // Anyway, below is a more-or-less acceptable initialization, per the SO
  // answer linked above.
  FLARE_INTERNAL_TLS_MODEL thread_local auto engine = [] {
    std::random_device r;
    std::seed_seq seeds{r(), r(), r(), r(), r(), r(), r(), r()};
    std::mt19937_64 engine(seeds);

    return engine;
  }();
  return engine;
}

}  // namespace random

// Generate a random value of type `T`. `T` may not be floating point types.
//
// Returns: A random value in range [std::numeric_limits<T>::min(),
//          std::numeric_limits<T>::max()] (both inclusive).
template <class T = std::uint64_t,
          class = std::enable_if_t<std::is_integral_v<T>>>
T Random() {
  return random::Engine()();
}

// Generate a random value in range [min, max] (inclusive) of type `T`.
//
// Values generated are distributed uniformly.
//
// `SimpleRandom` performs slightly better if multiple random values are needed.
// @sa: //common/random/simple_random.h.
template <class T>
T Random(T min, T max) {
  using Distribution = std::conditional_t<std::is_floating_point_v<T>,
                                          std::uniform_real_distribution<T>,
                                          std::uniform_int_distribution<T>>;
  Distribution dist(min, max);  // May NOT be `thread_local` as `min` / `max`
                                // could be different across calls.
  return dist(random::Engine());
}

// Shorthand for `Random(0, max)`.
template <class T>
T Random(T max) {
  return Random<T>(0, max);
}

}  // namespace flare

#endif  // FLARE_BASE_RANDOM_H_
