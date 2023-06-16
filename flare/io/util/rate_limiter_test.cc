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

#include <atomic>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "flare/base/chrono.h"
#include "flare/base/random.h"

using namespace std::literals;

namespace flare {

TEST(RateLimiter, TokenBucketRateLimiter) {
  TokenBucketRateLimiter limiter(1000, 1);
  std::size_t total = 0;
  auto start = ReadSteadyClock();

  while (ReadSteadyClock() - start < 5s) {
    auto current = limiter.GetQuota();
    total += current;
    limiter.ConsumeBytes(current);
    std::this_thread::sleep_for(1ms * Random(100));
  }

  ASSERT_NEAR(6000, total, 200);
}

TEST(RateLimiter, TokenBucketRateLimiter2) {
  TokenBucketRateLimiter limiter(1000, 1);
  std::size_t total = 0;
  auto start = ReadSteadyClock();

  while (ReadSteadyClock() - start < 5s) {
    auto current = limiter.GetQuota();
    total += current;
    limiter.ConsumeBytes(current);
    // We do not sleep this time.
  }

  ASSERT_NEAR(6000, total, 200);
}

TEST(RateLimiter, TokenBucketRateLimiterCapBurst) {
  TokenBucketRateLimiter limiter(25, 500);
  for (int i = 0; i != 10; ++i) {
    ASSERT_EQ(25, limiter.GetQuota());
    std::this_thread::sleep_for(10ms);
  }
}

TEST(RateLimiter, TokenBucketRateLimiterCapBurst2) {
  TokenBucketRateLimiter limiter(1000, 500);
  for (int i = 0; i != 10; ++i) {
    ASSERT_EQ(1000, limiter.GetQuota());
    std::this_thread::sleep_for(10ms);  // Enough to fully fill the bucket.
  }
}

TEST(RateLimiter, MultithreadedRateLimiter) {
  ThreadSafeRateLimiter limiter(
      std::make_unique<TokenBucketRateLimiter>(1000, 1));
  std::atomic<std::size_t> total = 0;
  std::vector<std::thread> ts;

  for (int i = 0; i != 10; ++i) {
    ts.emplace_back() = std::thread([&] {
      auto start = ReadSteadyClock();
      while (ReadSteadyClock() - start < 5s) {
        auto current = limiter.GetQuota();
        total += current;
        limiter.ConsumeBytes(current);
        std::this_thread::sleep_for(1ms * Random(10));
      }
    });
  }

  for (auto&& t : ts) {
    t.join();
  }

  ASSERT_NEAR(6000, total.load(), 500);
}

TEST(RateLimiter, LayeredRateLimiter) {
  ThreadSafeRateLimiter base_limiter(
      std::make_unique<TokenBucketRateLimiter>(1000, 1));
  auto our_limiter = std::make_unique<ThreadSafeRateLimiter>(
      std::make_unique<TokenBucketRateLimiter>(1000, 100));
  LayeredRateLimiter layered_limiter(&base_limiter, std::move(our_limiter));
  std::atomic<std::size_t> total = 0;
  std::vector<std::thread> ts;

  for (int i = 0; i != 10; ++i) {
    ts.emplace_back() = std::thread([&] {
      auto start = ReadSteadyClock();
      while (ReadSteadyClock() - start < 5s) {
        auto current = layered_limiter.GetQuota();
        total += current;
        layered_limiter.ConsumeBytes(current);
        std::this_thread::sleep_for(1ms * Random(10));
      }
    });
  }

  for (auto&& t : ts) {
    t.join();
  }

  ASSERT_NEAR(6000, total.load(), 500);  // `msrl` takes effect.
}

TEST(RateLimiter, LayeredRateLimiter2) {
  ThreadSafeRateLimiter base_limiter(
      std::make_unique<TokenBucketRateLimiter>(1000, 100));
  auto our_limiter = std::make_unique<ThreadSafeRateLimiter>(
      std::make_unique<TokenBucketRateLimiter>(1000, 1));
  LayeredRateLimiter layered_limiter(&base_limiter, std::move(our_limiter));
  std::atomic<std::size_t> total = 0;
  std::vector<std::thread> ts;

  for (int i = 0; i != 10; ++i) {
    ts.emplace_back() = std::thread([&] {
      auto start = ReadSteadyClock();
      while (ReadSteadyClock() - start < 5s) {
        auto current = layered_limiter.GetQuota();
        total += current;
        layered_limiter.ConsumeBytes(current);
        std::this_thread::sleep_for(1ms * Random(10));
      }
    });
  }

  for (auto&& t : ts) {
    t.join();
  }

  ASSERT_NEAR(6000, total.load(), 500);  // `tbsrl` takes effect.
}

TEST(RateLimiter, LayeredRateLimiter3) {
  ThreadSafeRateLimiter base_limiter(
      std::make_unique<TokenBucketRateLimiter>(1000, 1));
  std::atomic<std::size_t> total = 0;
  std::vector<std::thread> ts;

  for (int i = 0; i != 10; ++i) {
    ts.emplace_back() = std::thread([&] {
      auto our_limiter = std::make_unique<TokenBucketRateLimiter>(1000, 100);
      LayeredRateLimiter layered_limiter(&base_limiter, std::move(our_limiter));
      auto start = ReadSteadyClock();
      while (ReadSteadyClock() - start < 5s) {
        auto current = layered_limiter.GetQuota();
        total += current;
        layered_limiter.ConsumeBytes(current);
      }
    });
  }

  for (auto&& t : ts) {
    t.join();
  }

  ASSERT_NEAR(6000, total.load(), 500);  // `msrl` takes effect.
}

TEST(RateLimiter, LayeredRateLimiter4) {
  ThreadSafeRateLimiter base_limiter(
      std::make_unique<TokenBucketRateLimiter>(1000, 100));
  std::atomic<std::size_t> total = 0;
  std::vector<std::thread> ts;

  for (int i = 0; i != 10; ++i) {
    ts.emplace_back() = std::thread([&] {
      auto our_limiter = std::make_unique<ThreadSafeRateLimiter>(
          std::make_unique<TokenBucketRateLimiter>(1000, 1));
      LayeredRateLimiter layered_limiter(&base_limiter, std::move(our_limiter));
      auto start = ReadSteadyClock();
      while (ReadSteadyClock() - start < 5s) {
        auto current = layered_limiter.GetQuota();
        total += current;
        layered_limiter.ConsumeBytes(current);
      }
    });
  }

  for (auto&& t : ts) {
    t.join();
  }

  ASSERT_NEAR(60000, total.load(), 5000);  // `tbsrl` takes effect.
}

}  // namespace flare
