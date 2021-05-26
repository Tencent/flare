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

#ifndef FLARE_BASE_HAZPTR_HAZPTR_H_
#define FLARE_BASE_HAZPTR_HAZPTR_H_

#include "flare/base/hazptr/entry.h"
#include "flare/base/hazptr/entry_cache.h"
#include "flare/base/hazptr/hazptr_domain.h"
#include "flare/base/internal/logging.h"
#include "flare/base/internal/memory_barrier.h"
#include "flare/base/likely.h"

namespace flare {

// Class `Hazptr`, by itself, is not a "typed pointer". That is, no type
// information is associated with `Hazptr` instance. Rather, `Hazptr` is used
// for keeping a (typed) pointer alive.
//
// To use `Hazptr`, you instantiate an instance of it, and calls `Keep` to ask
// it to keep your pointer alive. The hazard pointer runtime guarantee you that
// before your `Hazptr` instance is destroyed, the pointer returned by `Keep`
// won't be destroyed by concurrent (or subsequent) calls to `Retire` (defined
// by `HazptrObject`, from which your type should be derived.).
//
// TODO(luobogao): Albeit being highly optimized, constructing a new `Hazptr`
// does cost some CPU cycles. We can provide a thread-local `Hazptr` for each
// thread (this should satisfy most use cases) to eliminate this overhead.
class Hazptr {
 public:
  template <class T>
  static constexpr auto is_hazptr_object_v =
      std::is_base_of_v<hazptr::Object, std::remove_const_t<T>>;

  // Construct a hazard pointer belonging to `domain`.
  explicit Hazptr(HazptrDomain* domain = hazptr::kDefaultDomainPlaceholder)
      : from_(domain), entry_(hazptr::GetEntryOf(from_)) {
    // FIXME: If we can somehow hint the compiler that `entry_->domain` is
    // `domain`, the compiler might be able to generate more optimized code.
  }

  Hazptr(Hazptr&& other) noexcept {
    from_ = other.from_;
    entry_ = std::exchange(other.entry_, nullptr);
  }

  Hazptr& operator=(Hazptr&& other) noexcept {
    if (FLARE_UNLIKELY(this == &other)) {
      return *this;
    }
    reset();
    entry_ = std::exchange(other.entry_, nullptr);
    return *this;
  }

  ~Hazptr() { reset(); }

  // Try to keep whatever in `ptr` alive. Pointer (if any) kept by this `Hazptr`
  // previously is impilicitly released.
  //
  // On success, the pointer being kepted by this `Hazptr` is returned,
  // otherwise `nullptr` is returned.
  template <class T, class = std::enable_if_t<is_hazptr_object_v<T>>>
  bool TryKeep(T** ptr, std::atomic<T*>* src) noexcept {
    auto p = *ptr;
    entry_->ExposePtr(p);
    internal::AsymmetricBarrierLight();
    *ptr = src->load(std::memory_order_acquire);
    if (FLARE_UNLIKELY(p != *ptr)) {
      entry_->ExposePtr(nullptr);
      return false;
    }
    // Otherwise we know that `src` is not changed before we finishing update
    // `Entry`. Given that all pointers in `Entry`s won't be reclaimed, we're
    // safe now.
    return true;
  }

  // Same as `TryKeep`, except that this one never fails.
  template <class T, class = std::enable_if_t<is_hazptr_object_v<T>>>
  T* Keep(std::atomic<T*>* src) noexcept {
    auto p = src->load(std::memory_order_relaxed);
    while (FLARE_UNLIKELY(!TryKeep(&p, src))) {
      // TODO(luobogao): Backoff.
    }
    return p;
  }

  // After `clear()`, no pointer is kept by this `Hazptr`. You may keep another
  // pointer alive by calling `Keep()` again.
  void clear() noexcept { entry_->ExposePtr(nullptr); }

 private:
  void reset() {
    if (FLARE_LIKELY(entry_)) {
      entry_->ExposePtr(nullptr);
      hazptr::PutEntryOf(from_, entry_);
      entry_ = nullptr;
    }
  }

 private:
  HazptrDomain* from_;  // For opt. reasons. It may or may not reflect the
                        // "actual" domain the `Entry` came from.
  hazptr::Entry* entry_;
};

}  // namespace flare

#endif  // FLARE_BASE_HAZPTR_HAZPTR_H_
