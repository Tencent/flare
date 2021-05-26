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

#ifndef FLARE_BASE_EXPERIMENTAL_SEQLOCKED_H_
#define FLARE_BASE_EXPERIMENTAL_SEQLOCKED_H_

#include <atomic>
#include <mutex>
#include <utility>

#include "flare/base/internal/memory_barrier.h"
#include "flare/base/likely.h"

namespace flare::experimental {

// Protects `T` with seqlock. I'm not sure this class name make sense though.
//
// @sa: https://en.wikipedia.org/wiki/Seqlock for what seqlock is.
//
// Implementation below is inspired by: https://github.com/rigtorp/Seqlock
//
// TODO(luobogao): TSan annotation.
template <class T, class M = std::mutex>
class Seqlocked {
 public:
  Seqlocked() = default;
  explicit Seqlocked(const T& value) : value_(value) {}

  // Load value stored in `*this`. This method always returns a consistent view
  // of a previous `Store` / `Update`.
  T Load() const noexcept;

  // Store value to `*this`. Protected by `M`.
  void Store(const T& value) noexcept;

  // Mutate value stored in `*this`. Protected by `M`.
  template <class F>
  void Update(F&& f) noexcept {
    std::scoped_lock _(writer_lock_);
    auto seq = seq_.load(std::memory_order_relaxed);
    seq_.store(seq + 1, std::memory_order_release);
    internal::WriteBarrier();
    std::forward<F>(f)(&value_);
    internal::WriteBarrier();
    seq_.store(seq + 2, std::memory_order_release);
  }

 private:
  // Load from / write to `value_` likely will annoy TSan, so we need to
  // annotate accesses to `value_`.
  //
  // Besides, without compiler barrier, the compiler may reorder access to
  // `value_` to other atomic accesses we made.
  T UnsafeLoadOrdered() const noexcept;
  void UnsafeStoreOrdered(const T& value) noexcept;

 private:
  M writer_lock_;
  // We use our own memory barrier when accessing `value_`. `seq_` provides
  // atomicity (but not memory visibility) only here.
  std::atomic<std::size_t> seq_{};
  T value_{};  // Value-initialized by default.
};

/////////////////////////////////
// Implementation goes below.  //
/////////////////////////////////

template <class T, class M>
inline T Seqlocked<T, M>::Load() const noexcept {
  T value;
  std::size_t seq1, seq2;
  do {
    seq1 = seq_.load(std::memory_order_relaxed);
    value = UnsafeLoadOrdered();
    seq2 = seq_.load(std::memory_order_relaxed);  // `relaxed`?
    if (FLARE_LIKELY(seq1 == seq2 && (seq1 % 2 == 0))) {
      return value;
    }
    // TODO(luobogao): Wait on writer's lock on failure?
  } while (true);
}

template <class T, class M>
inline void Seqlocked<T, M>::Store(const T& value) noexcept {
  std::scoped_lock _(writer_lock_);
  auto seq = seq_.load(std::memory_order_relaxed);
  seq_.store(seq + 1, std::memory_order_release);
  UnsafeStoreOrdered(value);
  seq_.store(seq + 2, std::memory_order_release);
}

template <class T, class M>
inline T Seqlocked<T, M>::UnsafeLoadOrdered() const noexcept {
  internal::ReadBarrier();  // Slow on AArch64, TBH.
  auto result = value_;
  internal::ReadBarrier();
  return result;
}

template <class T, class M>
inline void Seqlocked<T, M>::UnsafeStoreOrdered(const T& value) noexcept {
  internal::WriteBarrier();
  value_ = value;
  internal::WriteBarrier();
}

}  // namespace flare::experimental

#endif  // FLARE_BASE_EXPERIMENTAL_SEQLOCKED_H_
