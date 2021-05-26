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

#ifndef FLARE_IO_UTIL_RATE_LIMITER_H_
#define FLARE_IO_UTIL_RATE_LIMITER_H_

#include <limits>
#include <mutex>

#include "flare/base/maybe_owning.h"

namespace flare {

// This class controls bandwidth usage.
class RateLimiter {
 public:
  virtual ~RateLimiter() = default;

  // Called before reading / writing data. Returns maximum of bytes allowed to
  // read / write.
  //
  // If the caller wants to read / write more bytes than the number returned,
  // it's the caller's responsibility to re-call this method at a later time.
  virtual std::size_t GetQuota() = 0;

  // Called after actual read / write is performed. The actual number of bytes
  // read / written is given to the class.
  //
  // Note that this method could be called multiple times after a call to
  // `GetQuota()`. However, it always holds that the sum of all `consumed` will
  // be less or equal to the number returned by that method.
  //
  // Note that, however, if you're using the same limiter in multiple
  // connections for, e.g., limiting total bandwidth usage, it's possible that
  // all connections consume all the quota they get before other have a chance
  // to feedback their consumption, leading to over-consuming the quota. This
  // does not affect average bandwidth usage, but it does affect burst bandwidth
  // usage. Be prepared for this.
  virtual void ConsumeBytes(std::size_t consumed) = 0;

  // Rate limiter that applies overall rx bandwidth limitation.
  //
  // This particular implementation is thread-safe. (i.e., you can pass it to
  // several `StreamConnection` at the same time.)
  //
  // @sa `rate_limiter.cc` for flags controlling this rate limiter.
  static RateLimiter* GetDefaultRxRateLimiter();
  // Same as `GetDefaultRxRateLimiter()` but this one controls tx speed.
  static RateLimiter* GetDefaultTxRateLimiter();
};

// Rate limiter implemented via token bucket.
class TokenBucketRateLimiter : public RateLimiter {
 public:
  // `burst_quota`: Maximum number of bytes allowed in burst case. This
  //                parameter caps the maximum number could `GetQuota` return.
  //                (i.e., burst bandwidth usage.)
  //
  // `quota_per_tick`: When quota does not reach its maximum, this parameter
  //                   specifies how much quota is replenished per time unit.
  //                   (i.e., average bandwidth usage.)
  TokenBucketRateLimiter(
      std::size_t burst_quota, std::size_t quota_per_tick,
      std::chrono::nanoseconds tick = std::chrono::milliseconds(1),
      bool over_consumption_allowed = true);

  // Get maximum number of bytes allowed to read / write.
  std::size_t GetQuota() override;

  // Feedback how many bytes have been read / written.
  void ConsumeBytes(std::size_t consumed) override;

 private:
  std::size_t max_quota_;
  std::size_t quota_per_tick_;
  std::chrono::nanoseconds tick_;
  bool over_consumption_allowed_;

  // Last time `curr_quota_` is re-filled. The timestamp is represented as
  // number of `tick_`s since epoch.
  std::uint64_t last_refill_;
  // Can be negative if the quota was over-consumed.
  std::int64_t curr_quota_{0};
};

// This class synchronizes calls to the limiter it holds. Note that the limiter
// given to this class should must be tolerant to over-consumption.
//
// Obviously, this class does not scales well due to its internal lock.
class ThreadSafeRateLimiter : public RateLimiter {
 public:
  // `burst_limit` caps the upper bound of return value of `GetQuota`, this
  // helps in mitigating over-consumption in certain cases.
  explicit ThreadSafeRateLimiter(
      MaybeOwning<RateLimiter> limiter,
      std::size_t burst_limit = std::numeric_limits<std::size_t>::max());

  std::size_t GetQuota() override;

  void ConsumeBytes(std::size_t consumed) override;

 private:
  std::size_t burst_limit_;
  std::mutex lock_;
  MaybeOwning<RateLimiter> impl_;
};

// Multiple layered rate limiter. It does not only respect its own limitation,
// but also it's upper layer's.
//
// This limiter could be used for, e.g., limiting both single connection's
// bandwidth usage as well as whole program's.
class LayeredRateLimiter : public RateLimiter {
 public:
  // No I'm not interested in accepting `std::shared_ptr<...>` as `upper`. Keep
  // it alive yourself.
  LayeredRateLimiter(RateLimiter* upper, MaybeOwning<RateLimiter> ours);

  // Returns maximum number of bytes allowed to read / write. The smaller one of
  // `upper` and `ours` is returned.
  std::size_t GetQuota() override;

  // Feedback `consumed` to both `upper` and `ours`.
  void ConsumeBytes(std::size_t consumed) override;

 private:
  RateLimiter* upper_;
  MaybeOwning<RateLimiter> ours_;
};

}  // namespace flare

#endif  // FLARE_IO_UTIL_RATE_LIMITER_H_
