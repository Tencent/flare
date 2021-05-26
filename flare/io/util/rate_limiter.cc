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

#include "flare/io/util/rate_limiter.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include "thirdparty/gflags/gflags.h"

#include "flare/base/chrono.h"
#include "flare/base/logging.h"
#include "flare/base/string.h"

// These two flags control bandwidth usage of the program. If set to zero, no
// limit is applied.
//
// @sa: https://en.wikipedia.org/wiki/Data-rate_units for data-rate unit
// symbols.
DEFINE_string(
    flare_io_cap_rx_bandwidth, "0",
    "It non-zero, this flag caps receive speed of the program. This option is "
    "specified in bit/s by default. You can use suffix 'K', 'M' or 'G' to "
    "specify this option in Kbps, Mbps, Gbps respectively.");
DEFINE_string(flare_io_cap_tx_bandwidth, "0",
              "Same as `flare_io_cap_rx_bandwitch`, except for this one "
              "controls send speed.");

using namespace std::literals;

namespace flare {

namespace {

class NullLimiter : public RateLimiter {
 public:
  std::size_t GetQuota() override {
    return std::numeric_limits<std::size_t>::max();
  }

  void ConsumeBytes(std::size_t consumed) override {}
};

std::uint64_t ParseToBps(std::string s) {
  FLARE_CHECK(
      !s.empty(),
      "`flare_io_cap_*_bandwidth` may not be left empty. If no limitation "
      "should be applied, do not specify it in command line.");
  static const std::unordered_map<char, std::uint64_t> kScales = {
      {'K', 1000}, {'M', 1000 * 1000}, {'G', 1000 * 1000 * 1000}};
  std::size_t scale = 1;
  if (auto iter = kScales.find(s.back()); iter != kScales.end()) {
    scale = iter->second;
    s.pop_back();
  }
  auto base = TryParse<std::uint64_t>(s);
  FLARE_CHECK(base, "One (or both) of `flare_io_cap_*_bandwidth` is invalid.");
  FLARE_CHECK(*base * scale / scale == *base,
              "One (or both) of `flare_io_cap_*_bandwidth` is too large.");
  return *base * scale / 8;
}

template <class Tag>
RateLimiter* GetRateLimiterOf(std::uint64_t bps) {
  if (!bps) {
    static NullLimiter null_limiter;
    return &null_limiter;
  } else {
    static ThreadSafeRateLimiter limiter(
        std::make_unique<TokenBucketRateLimiter>(bps, bps / (1s / 1ms)),
        bps / 10);
    return &limiter;
  }
}

}  // namespace

RateLimiter* RateLimiter::GetDefaultRxRateLimiter() {
  struct Tag;
  static RateLimiter* limiter =
      GetRateLimiterOf<Tag>(ParseToBps(FLAGS_flare_io_cap_rx_bandwidth));
  return limiter;
}

RateLimiter* RateLimiter::GetDefaultTxRateLimiter() {
  struct Tag;
  static RateLimiter* limiter =
      GetRateLimiterOf<Tag>(ParseToBps(FLAGS_flare_io_cap_tx_bandwidth));
  return limiter;
}

TokenBucketRateLimiter::TokenBucketRateLimiter(std::size_t burst_quota,
                                               std::size_t quota_per_tick,
                                               std::chrono::nanoseconds tick,
                                               bool over_consumption_allowed)
    : max_quota_(burst_quota),
      quota_per_tick_(quota_per_tick),
      tick_(tick),
      over_consumption_allowed_(over_consumption_allowed) {
  FLARE_CHECK_GT(burst_quota, 0);
  FLARE_CHECK_GT(quota_per_tick, 0);
  last_refill_ = ReadSteadyClock().time_since_epoch() / tick_;
  curr_quota_ = max_quota_;  // Initially we're full of tokens.
}

std::size_t TokenBucketRateLimiter::GetQuota() {
  // Let's refill the tokens first.
  auto now = ReadSteadyClock().time_since_epoch() / tick_;
  auto last_refill = std::exchange(last_refill_, now);
  curr_quota_ += quota_per_tick_ * (now - last_refill);

  if (curr_quota_ > 0) {
    // Cap it to `max_quota_`.
    //
    // Converting it to `std::size_t` won't underflow as it's positive.
    curr_quota_ = std::min<std::size_t>(curr_quota_, max_quota_);
    return curr_quota_;
  } else {
    return 0;  // Nothing to cap, no quota to return.
  }
}

void TokenBucketRateLimiter::ConsumeBytes(std::size_t consumed) {
  FLARE_CHECK(over_consumption_allowed_ || consumed <= curr_quota_);
  curr_quota_ -= consumed;
}

ThreadSafeRateLimiter::ThreadSafeRateLimiter(MaybeOwning<RateLimiter> limiter,
                                             std::size_t burst_limit)
    : burst_limit_(burst_limit), impl_(std::move(limiter)) {
  FLARE_CHECK_GT(burst_limit, 0);
}

std::size_t ThreadSafeRateLimiter::GetQuota() {
  std::scoped_lock lk(lock_);
  return std::min(burst_limit_, impl_->GetQuota());
}

void ThreadSafeRateLimiter::ConsumeBytes(std::size_t consumed) {
  std::scoped_lock lk(lock_);
  return impl_->ConsumeBytes(consumed);
}

LayeredRateLimiter::LayeredRateLimiter(RateLimiter* upper,
                                       MaybeOwning<RateLimiter> ours)
    : upper_(upper), ours_(std::move(ours)) {}

std::size_t LayeredRateLimiter::GetQuota() {
  return std::min(upper_->GetQuota(), ours_->GetQuota());
}

void LayeredRateLimiter::ConsumeBytes(std::size_t consumed) {
  upper_->ConsumeBytes(consumed);
  ours_->ConsumeBytes(consumed);
}

}  // namespace flare
