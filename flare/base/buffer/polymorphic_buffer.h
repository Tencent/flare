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

#ifndef FLARE_BASE_BUFFER_POLYMORPHIC_BUFFER_H_
#define FLARE_BASE_BUFFER_POLYMORPHIC_BUFFER_H_

#include <atomic>
#include <chrono>
#include <limits>
#include <utility>

#include "flare/base/internal/singly_linked_list.h"
#include "flare/base/object_pool.h"
#include "flare/base/ref_ptr.h"

namespace flare {

namespace detail {

struct PolymorphicBufferBlockDeleter;

}  // namespace detail

// This is the base class for all buffer blocks we recognizes.
//
// Depending on how the actual buffer is allocated, it can be:
//
// - A buffer allocated from our object pool.
// - Referencing some "foreign" buffer from user.
// - Owns a "non-native" buffer.
//
// If desired, user of Flare is allowed to implement this interface to integrate
// their own buffer with Flare.
class PolymorphicBufferBlock
    : public RefCounted<PolymorphicBufferBlock,
                        detail::PolymorphicBufferBlockDeleter> {
 public:
  virtual ~PolymorphicBufferBlock() = default;

  // Using snake_case here to be consistent with STL's convention, not sure if
  // we should use CamelCase to be consistent with Tencent's convention instead.

  // Returns a pointer to data kept by this buffer block.
  virtual const char* data() const noexcept = 0;

  // Returns the size of the buffer block.
  //
  // TBH, we don't need this method. No one else in Flare cares about size of a
  // buffer block, all they'd like to know is the size of `PolymorphicBuffer`.
  virtual std::size_t size() const noexcept = 0;

  // Called upon destruction. This method is responsible for reclaim any
  // resources allocated to this object (including memory allocated to the
  // object itself.).
  //
  // The default implementation should serve you well unless your buffer is
  // allocated from some customized allocated instead of `new T`.
  //
  // Some "native" buffers use object pool to speed up allocation. These class
  // override this method so as not to be destroyed when ref-count reaches zero.
  //
  // For the moment, we reset ref-count to 1 before calling this method. THIS IS
  // AN IMPLEMENTATION DETAIL AND MAY SUBJECT TO CHANGE. If you want to reset
  // ref-count to a certain value, do it yourself in `Destroy()`.
  virtual void Destroy() noexcept { delete this; }
};

// This structure describe a *portion* (contiguous part) of some type of buffer
// block (subclass of `PolymorphicBufferBlock`).
class PolymorphicBuffer {
 public:
  PolymorphicBuffer() = default;
  PolymorphicBuffer(const PolymorphicBuffer&) = default;
  PolymorphicBuffer& operator=(const PolymorphicBuffer&) = default;

  PolymorphicBuffer(PolymorphicBuffer&& other) noexcept
      : ptr_(other.ptr_), size_(other.size_), ref_(std::move(other.ref_)) {
    other.Clear();
  }
  PolymorphicBuffer& operator=(PolymorphicBuffer&& other) noexcept {
    if (this != &other) {
      ptr_ = other.ptr_;
      size_ = other.size_;
      ref_ = std::move(other.ref_);
      other.Clear();
    }
    return *this;
  }

  // Same as constructing a new one and call `Reset` on it.
  PolymorphicBuffer(RefPtr<PolymorphicBufferBlock> data, std::size_t start,
                    std::size_t size)
      : ptr_(data->data() + start), size_(size), ref_(std::move(data)) {}

  // Accessor.
  const char* data() const noexcept { return ptr_; }
  std::size_t size() const noexcept { return size_; }

  // Changes the portion of buffer we're seeing.
  void Skip(std::size_t bytes) {
    FLARE_DCHECK_LT(bytes, size_);
    size_ -= bytes;
    ptr_ += bytes;
  }
  void set_size(std::size_t size) {
    FLARE_DCHECK_LE(size, size_);
    size_ = size;
  }

  // Accepts a new buffer block.
  void Reset(RefPtr<PolymorphicBufferBlock> data, std::size_t start,
             std::size_t size) {
    FLARE_DCHECK_LE(start, size);
    FLARE_DCHECK_LE(size, data->size());
    ref_ = std::move(data);
    ptr_ = ref_->data() + start;
    size_ = size;
  }

  // Resets everything.
  void Clear() {
    ptr_ = nullptr;
    size_ = 0;
    ref_ = nullptr;
  }

 private:
  friend class NoncontiguousBuffer;

  internal::SinglyLinkedListEntry chain;
  const char* ptr_{};
  std::size_t size_{};
  RefPtr<PolymorphicBufferBlock> ref_;
};

namespace detail {

struct PolymorphicBufferBlockDeleter {
  void operator()(PolymorphicBufferBlock* p) {
    FLARE_DCHECK_EQ(p->UnsafeRefCount(), 0);
    p->ref_count_.store(1, std::memory_order_relaxed);  // Rather hacky.
    p->Destroy();
  }
};

}  // namespace detail

}  // namespace flare

namespace flare {

template <>
struct PoolTraits<PolymorphicBuffer> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 32768;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 8192;
  static constexpr auto kTransferBatchSize = 1024;

  static void OnPut(PolymorphicBuffer* bb) {
    bb->Clear();  // We don't need the data to be kept.
  }
};

}  // namespace flare

#endif  // FLARE_BASE_BUFFER_POLYMORPHIC_BUFFER_H_
