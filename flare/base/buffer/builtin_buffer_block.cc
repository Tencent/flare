// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/base/buffer/builtin_buffer_block.h"

#include <string>
#include <unordered_map>

#include "gflags/gflags.h"

#include "flare/base/object_pool.h"
#include "flare/init/on_init.h"

DEFINE_string(flare_buffer_block_size, "4K",
              "Determines byte size of a single buffer block used by Flare. "
              "Valid choices are 4K, 64K, 1M. Note that setting this option "
              "incorrectly can lead to excessive memory usage (Possibly due to "
              "buffer block underutilization).");

namespace flare {

namespace {

template <std::size_t kSize>
struct alignas(hardware_destructive_interference_size) FixedNativeBufferBlock
    : NativeBufferBlock {
  char* mutable_data() noexcept override { return buffer.data(); }
  const char* data() const noexcept override { return buffer.data(); }
  std::size_t size() const noexcept override { return buffer.size(); }
  void Destroy() noexcept override {
    object_pool::Put<FixedNativeBufferBlock>(this);
  }

  static constexpr auto kBufferSize = kSize - sizeof(NativeBufferBlock);
  std::array<char, kBufferSize> buffer;
};

template <std::size_t kSize>
RefPtr<NativeBufferBlock> MakeNativeBufferBlockOfBytes() {
  return RefPtr(adopt_ptr,
                object_pool::Get<FixedNativeBufferBlock<kSize>>().Leak());
}

// We initialize this function with a default value so that UTs will always
// work, even without touching FLARE_TEST_MAIN.
RefPtr<NativeBufferBlock> (*make_native_buffer_block)() =
    MakeNativeBufferBlockOfBytes<4096>;

void InitializeMakeNativeBufferBlock() {
  // What about case sensitivity?
  static const std::unordered_map<std::string, RefPtr<NativeBufferBlock> (*)()>
      kMakers = {{"4K", MakeNativeBufferBlockOfBytes<4096>},
                 {"64K", MakeNativeBufferBlockOfBytes<65536>},
                 {"1M", MakeNativeBufferBlockOfBytes<1048576>}};
  if (auto iter = kMakers.find(FLAGS_flare_buffer_block_size);
      iter != kMakers.end()) {
    make_native_buffer_block = iter->second;
  } else {
    FLARE_UNEXPECTED(
        "Unexpected buffer block size [{}]. Only 4K/64K/1M buffer block is "
        "supported.",
        FLAGS_flare_buffer_block_size);
  }
}

FLARE_ON_INIT(0, InitializeMakeNativeBufferBlock);

}  // namespace

RefPtr<NativeBufferBlock> MakeNativeBufferBlock() {
  return make_native_buffer_block();
}

}  // namespace flare

namespace flare {

template <>
struct PoolTraits<FixedNativeBufferBlock<4096>> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 16384;  // 64M per node.
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 4096;  // 16M per thread.
  static constexpr auto kTransferBatchSize = 1024;       // Extra 4M.
};

template <>
struct PoolTraits<FixedNativeBufferBlock<65536>> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 1024;  // 64M per node.
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 256;  // 16M per thread.
  static constexpr auto kTransferBatchSize = 64;        // Extra 4M.
};

template <>
struct PoolTraits<FixedNativeBufferBlock<1048576>> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 128;  // 128M per node.
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 64;  // 64M per thread.
  static constexpr auto kTransferBatchSize = 16;       // Extra 16M.
};

}  // namespace flare
