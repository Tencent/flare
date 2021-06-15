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

#include "flare/rpc/binlog/dumper.h"

#include <limits>
#include <memory>
#include <random>

#include "gflags/gflags.h"

#include "flare/base/id_alloc.h"
#include "flare/base/never_destroyed.h"
#include "flare/base/string.h"
#include "flare/rpc/binlog/tags.h"
#include "flare/rpc/internal/sampler.h"

using namespace std::literals;

DEFINE_string(
    flare_binlog_dumper, "",
    "Name of binlog dumper. To (selectively) dump requests processed, and "
    "possibly later use them to perform a dry-run, you can use a dumper that "
    "comforts you here. By default nothing is dumped.");
DEFINE_int32(flare_binlog_dumper_sampling_interval, 0,
             "Minimum milliseconds between two RPCs are sampled. For perf. "
             "reasons, don't set it too high. This parameter cannot be used "
             "simultaneously with `flare_binlog_dumper_sampling_every_n`.");
DEFINE_int32(flare_binlog_dumper_sampling_every_n, 0,
             "If non-zero, this parameter specifies desired sampling ratio in "
             "terms of 1/N-th. This parameter cannot be used simultaneously "
             "with `flare_binlog_dumper_sampling_interval`.");

namespace flare::binlog {

FLARE_DEFINE_CLASS_DEPENDENCY_REGISTRY(dumper_registry, Dumper);

namespace {

std::unique_ptr<Dumper> CreateDumperFromFlags() {
  if (FLAGS_flare_binlog_dumper.empty()) {
    return nullptr;
  }
  FLARE_LOG_INFO("Using binlog dumper [{}] to dump RPCs.",
                 FLAGS_flare_binlog_dumper);
  auto v = dumper_registry.New(FLAGS_flare_binlog_dumper);
  return v;
}

struct CorrelationIdTraits {
  using Type = std::uint64_t;
  static constexpr auto kMin = 1;
  static constexpr auto kMax = std::numeric_limits<std::uint64_t>::max();
  static constexpr auto kBatchSize = 1048576;
};

}  // namespace

bool AcquireSamplingQuotaForDumping() {
  if (FLAGS_flare_binlog_dumper.empty()) {
    return false;
  }

  static NeverDestroyed<std::unique_ptr<rpc::detail::Sampler>> sampler(
      []() -> std::unique_ptr<rpc::detail::Sampler> {
        FLARE_CHECK(!!FLAGS_flare_binlog_dumper_sampling_interval ^
                        !!FLAGS_flare_binlog_dumper_sampling_every_n,
                    "One and one only one of sampling strategy can be set.");
        if (FLAGS_flare_binlog_dumper_sampling_interval) {
          return std::make_unique<rpc::detail::LargeIntervalSampler>(
              FLAGS_flare_binlog_dumper_sampling_interval * 1ms);
        } else if (FLAGS_flare_binlog_dumper_sampling_every_n) {
          return std::make_unique<rpc::detail::EveryNSampler>(
              FLAGS_flare_binlog_dumper_sampling_every_n);
        } else {
          FLARE_LOG_INFO(
              "Neither `flare_binlog_dumper_sampling_interval` nor "
              "`flare_binlog_dumper_sampling_every_n` is set, defaulting to "
              "sampling one RPC per second.");
          return std::make_unique<rpc::detail::LargeIntervalSampler>(1s);
        }
      }());
  return (*sampler)->Sample();
}

std::string NewCorrelationId() {
  static const std::string kRandomPrefix =
      Format("{:08x}{:08x}", static_cast<std::uint32_t>(std::random_device{}()),
             static_cast<std::uint32_t>(std::random_device{}()));

  return Format("{}{:016x}", kRandomPrefix,
                id_alloc::Next<CorrelationIdTraits>());
}

Dumper* GetDumper() {
  static NeverDestroyed<std::unique_ptr<Dumper>> dumper(
      CreateDumperFromFlags());
  return dumper->get();
}

}  // namespace flare::binlog
