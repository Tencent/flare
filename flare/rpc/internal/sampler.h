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

#ifndef FLARE_RPC_INTERNAL_SAMPLER_H_
#define FLARE_RPC_INTERNAL_SAMPLER_H_

#include <atomic>
#include <chrono>

#include "flare/base/chrono.h"
#include "flare/base/likely.h"
#include "flare/base/thread/thread_local.h"

namespace flare::rpc::detail {

// Interface of sampler.
class Sampler {
 public:
  virtual ~Sampler() = default;

  virtual bool Sample() noexcept = 0;
};

// This class implements an efficient sampler when sampling with a large
// interval (in tens or hundreds of milliseconds).
//
// If you're sampling with a small interval / high possibility, this class does
// not suite your needs.
class LargeIntervalSampler : public Sampler {
 public:
  explicit LargeIntervalSampler(std::chrono::nanoseconds interval)
      : interval_(interval) {}

  // Returns true if this one should be sampled.
  bool Sample() noexcept override {
    auto now = ReadCoarseSteadyClock().time_since_epoch();
    auto t = next_sampled_.load(std::memory_order_relaxed);
    if (FLARE_UNLIKELY(t <= now)) {
      auto next = now + interval_;
      return next_sampled_.compare_exchange_strong(t, next,
                                                   std::memory_order_relaxed);
    }
    return false;
  }

 private:
  std ::atomic<std::chrono::nanoseconds> next_sampled_{};
  std::chrono::nanoseconds interval_;
};

// This class implements a sampler that samples after every N tries.
class EveryNSampler : public Sampler {
 public:
  explicit EveryNSampler(std::uint64_t n) : n_(n) { FLARE_CHECK_GT(n, 0); }

  bool Sample() noexcept override {
    auto&& e = *occurs_;
    if (FLARE_UNLIKELY(++e >= n_)) {
      FLARE_CHECK_EQ(e, n_);
      e = 0;
      return true;
    }
    return false;
  }

 private:
  std::uint64_t n_;
  flare::internal::ThreadLocalAlwaysInitialized<std::uint64_t> occurs_;
};

}  // namespace flare::rpc::detail

#endif  // FLARE_RPC_INTERNAL_SAMPLER_H_
