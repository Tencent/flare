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

#ifndef FLARE_BASE_INTERNAL_CIRCULAR_BUFFER_H_
#define FLARE_BASE_INTERNAL_CIRCULAR_BUFFER_H_

#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "flare/base/likely.h"

namespace flare::internal {

// Circular buffer (a bounded **SPSC** queue).
//
// FOR INTERNAL USE ONLY. Specialized for our use-case.
//
// NOT optimized for concurrent use, it's primarily used for deferring
// operations (i.e., `Push` is called frequently, while `Pop` is only called
// periodically.).
template <class T>
class CircularBuffer {
  // No we don't take false-sharing or whatever destructive inference between
  // cores here, as `Pop()` is not intended to be called often. (As we've stated
  // in header comments of this class, it's specialized for our use-case.)
  class UninitializedObject;  // Behaves more like a `struct` TBH.

 public:
  explicit CircularBuffer(std::size_t capacity)
      : capacity_(capacity + 1 /* A slot is reserved for sentinel. */),
        objects_(std::make_unique<UninitializedObject[]>(capacity_)) {}

  ~CircularBuffer() {
    [[maybe_unused]] std::vector<T> dummy;
    Pop(&dummy);  // Destroy all objects held by us.
  }

  template <class... Ts>
  bool Emplace(Ts&&... args) {
    auto head = head_.load(std::memory_order_relaxed);
    auto next = NormalizeIndex(head + 1);
    // Read with acquire semantic on `tail_` guarantees us that we can see any
    // `std::move(...)` (away) done by `Pop()`.
    if (FLARE_UNLIKELY(next == tail_.load(std::memory_order_acquire))) {
      return false;
    }
    objects_[head].Initialize(std::forward<Ts>(args)...);
    // We're the only producer, no need to use expensive RMW here.
    head_.store(next, std::memory_order_release);  // Let `Pop()` see the value.
    return true;
  }

  // As an perf. optimization advice, you can use a `thread_local` vector to
  // hold the result (so as not to allocate vector's internal storage each
  // time.).
  //
  // IT'S CALLER'S RESPONSIBILITY TO CLEAR `OBJECTS` BEFORE CALLING THIS METHOD.
  void Pop(std::vector<T>* objects) {
    auto upto = head_.load(std::memory_order_acquire);  // Pairs with `Push()`.
    auto current = tail_.load(std::memory_order_relaxed);
    while (current != upto) {
      // Destructive move would helper but it's not standardized yet (if ever).
      objects->push_back(std::move(*objects_[current].Get()));
      objects_[current].Destroy();
      current = NormalizeIndex(current + 1);
    }
    // Let `Push()` see the moves we've done.
    tail_.store(current, std::memory_order_release);
  }

 private:
  class UninitializedObject {
   public:
    T* Get() noexcept { return reinterpret_cast<T*>(&storage_); }

    template <class... Ts>
    void Initialize(Ts&&... args) {
      new (Get()) T(std::forward<Ts>(args)...);
    }

    void Destroy() { Get()->~T(); }

   private:
    std::aligned_storage_t<sizeof(T), alignof(T)> storage_;
  };

  std::size_t NormalizeIndex(std::size_t x) {
    // Faster than `x % capacity_` as we know it can't be more than `2 *
    // capacity_`.
    return FLARE_LIKELY(x < capacity_) ? x : x - capacity_;
  }

 private:
  std::size_t capacity_;
  std::unique_ptr<UninitializedObject[]> objects_;
  std::atomic<std::size_t> head_{}, tail_{};
};

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_CIRCULAR_BUFFER_H_
