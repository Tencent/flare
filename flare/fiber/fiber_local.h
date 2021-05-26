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

#ifndef FLARE_FIBER_FIBER_LOCAL_H_
#define FLARE_FIBER_FIBER_LOCAL_H_

#include "flare/base/internal/index_alloc.h"
#include "flare/fiber/detail/fiber_entity.h"

namespace flare {

namespace fiber::detail {

struct FiberLocalIndexTag;
struct TrivialFiberLocalIndexTag;

}  // namespace fiber::detail

// `T` needs to be `DefaultConstructible`.
//
// You should normally use this class as static / member variable. In case of
// variable in stack, just use automatic variable (stack variable) instead.
template <class T>
class FiberLocal {
  // @sa: Comments in `FiberEntity` for definition of "trivial" here.
  inline static constexpr auto is_using_trivial_fls_v =
      std::is_trivial_v<T> &&
      sizeof(T) <= sizeof(fiber::detail::FiberEntity::trivial_fls_t);

 public:
  // A dedicated FLS slot is allocated for this `FiberLocal`.
  FiberLocal() : slot_index_(GetIndexAlloc()->Next()) {}

  // The FLS slot is released on destruction.
  ~FiberLocal() { GetIndexAlloc()->Free(slot_index_); }

  // Accessor.
  T* operator->() const noexcept { return get(); }
  T& operator*() const noexcept { return *get(); }
  T* get() const noexcept { return Get(); }

 private:
  T* Get() const noexcept {
    auto current_fiber = fiber::detail::GetCurrentFiberEntity();
    if constexpr (is_using_trivial_fls_v) {
      return reinterpret_cast<T*>(current_fiber->GetTrivialFls(slot_index_));
    } else {
      auto ptr = current_fiber->GetFls(slot_index_);
      if (!*ptr) {
        *ptr = MakeErased<T>();
      }
      return static_cast<T*>(ptr->Get());
    }
  }

  static internal::IndexAlloc* GetIndexAlloc() {
    if constexpr (is_using_trivial_fls_v) {
      return internal::IndexAlloc::For<
          fiber::detail::TrivialFiberLocalIndexTag>();
    } else {
      return internal::IndexAlloc::For<fiber::detail::FiberLocalIndexTag>();
    }
  }

 private:
  std::size_t slot_index_;
};

}  // namespace flare

#endif  // FLARE_FIBER_FIBER_LOCAL_H_
