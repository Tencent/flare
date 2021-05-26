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

#ifndef FLARE_BASE_WRITE_MOSTLY_BASIC_OPS_H_
#define FLARE_BASE_WRITE_MOSTLY_BASIC_OPS_H_

#include <atomic>
#include <limits>

#include "flare/base/write_mostly/write_mostly.h"

namespace flare {

namespace write_mostly::detail {

// Wrapper of single value T with different Op.
// Inspired from bvar : For performance issues, we don't let Op return value,
// instead it shall set the result to the first parameter in-place.
// Namely to add two values "+=" should be implemented rather than "+".
template <class T, class Op>
struct CumulativeTraits {
  using Type = T;
  using WriteBuffer = std::atomic<T>;
  static constexpr auto kWriteBufferInitializer = Op::kIdentity;
  static void Update(WriteBuffer* wb, const T& val) { Op()(wb, val); }
  static void Merge(WriteBuffer* wb1, const WriteBuffer& wb2) {
    Op()(wb1, wb2);
  }
  static void Copy(const WriteBuffer& src_wb, WriteBuffer* dst_wb) {
    dst_wb->store(src_wb.load(std::memory_order_relaxed),
                  std::memory_order_relaxed);
  }
  static T Read(const WriteBuffer& wb) {
    return wb.load(std::memory_order_relaxed);
  }
};

template <class T>
struct OpAdd {
  static constexpr auto kIdentity = T();
  void operator()(std::atomic<T>* l, const std::atomic<T>& r) const {
    // No costly RMW here.
    l->store(
        l->load(std::memory_order_relaxed) + r.load(std::memory_order_relaxed),
        std::memory_order_relaxed);
  }
};

template <class T>
using AddTraits = CumulativeTraits<T, OpAdd<T>>;

template <class T>
struct OpMin {
  static constexpr auto kIdentity = std::numeric_limits<T>::max();
  void operator()(std::atomic<T>* l, const std::atomic<T>& r) {
    if (auto v = r.load(std::memory_order_relaxed);
        v < l->load(std::memory_order_relaxed)) {
      l->store(v, std::memory_order_relaxed);
    }
  }
};

template <class T>
using MinTraits = CumulativeTraits<T, OpMin<T>>;

template <class T>
struct OpMax {
  static constexpr auto kIdentity = std::numeric_limits<T>::min();
  void operator()(std::atomic<T>* l, const std::atomic<T>& r) const {
    if (auto v = r.load(std::memory_order_relaxed);
        v > l->load(std::memory_order_relaxed)) {
      l->store(v, std::memory_order_relaxed);
    }
  }
};

template <class T>
using MaxTraits = CumulativeTraits<T, OpMax<T>>;

template <class T>
struct AvgTraits {
  struct AvgBuffer {
    T val_;
    std::size_t num_;
  };
  using Type = T;
  using WriteBuffer = AvgBuffer;
  static constexpr auto kWriteBufferInitializer = AvgBuffer{T(), 0};
  static void Update(WriteBuffer* wb, const T& val) {
    wb->val_ += val;
    ++wb->num_;
  }
  static void Merge(WriteBuffer* wb1, const WriteBuffer& wb2) {
    wb1->val_ += wb2.val_;
    wb1->num_ += wb2.num_;
  }
  static void Copy(const WriteBuffer& src_wb, WriteBuffer* dst_wb) {
    *dst_wb = src_wb;
  }
  static T Read(const WriteBuffer& wb) {
    return wb.num_ ? (wb.val_ / wb.num_) : 0;
  }
};

}  // namespace write_mostly::detail

// An optimized-for-writer thread-safe counter.
//
// I don't see a point in using distinct class for "Counter" and "Gauge", but to
// keep the naming across our library consistent, let's separate them.
template <class T, class Base = WriteMostly<write_mostly::detail::AddTraits<T>>>
class WriteMostlyCounter : private Base {
 public:
  void Add(T value) noexcept {
    FLARE_CHECK(value >= 0);
    Base::Update(value);
  }
  void Increment() noexcept { Add(1); }

  T Read() const noexcept { return Base::Read(); }
  void Reset() noexcept { Base::Reset(); }  // NOT thread-safe.
};

// Same as `WriteMostlyCounter` except that values in it can be decremented.
template <class T, class Base = WriteMostly<write_mostly::detail::AddTraits<T>>>
class WriteMostlyGauge : private Base {
 public:
  void Add(T value) noexcept {
    FLARE_CHECK(value >= 0);
    Base::Update(value);
  }

  void Subtract(T value) noexcept {
    FLARE_CHECK(value >= 0);
    Base::Update(-value);
  }

  void Increment() noexcept { Add(1); }
  void Decrement() noexcept { Subtract(1); }

  T Read() const noexcept { return Base::Read(); }
  void Reset() noexcept { Base::Reset(); }  // NOT thread-safe.
};

// An optimized-for-writer thread-safe minimizer.
template <class T, class Base = WriteMostly<write_mostly::detail::MinTraits<T>>>
class WriteMostlyMiner : private Base {
 public:
  void Update(T value) noexcept { Base::Update(value); }
  T Read() const noexcept { return Base::Read(); }
  void Reset() noexcept { Base::Reset(); }  // NOT thread-safe.
};

// An optimized-for-writer thread-safe maximizer.
template <class T, class Base = WriteMostly<write_mostly::detail::MaxTraits<T>>>
class WriteMostlyMaxer : private Base {
 public:
  void Update(T value) noexcept { Base::Update(value); }
  T Read() const noexcept { return Base::Read(); }
  void Reset() noexcept { Base::Reset(); }  // NOT thread-safe.
};

// An optimized-for-writer thread-safe averager.
template <class T, class Base = WriteMostly<write_mostly::detail::AvgTraits<T>>>
class WriteMostlyAverager : private Base {
 public:
  void Update(T value) noexcept { Base::Update(value); }
  T Read() const noexcept { return Base::Read(); }
  void Reset() noexcept { Base::Reset(); }  // NOT thread-safe.
};

}  // namespace flare

#endif  // FLARE_BASE_WRITE_MOSTLY_BASIC_OPS_H_
