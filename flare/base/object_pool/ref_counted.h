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

#ifndef FLARE_BASE_OBJECT_POOL_REF_COUNTED_H_
#define FLARE_BASE_OBJECT_POOL_REF_COUNTED_H_

#include "flare/base/object_pool.h"
#include "flare/base/ref_ptr.h"

namespace flare::object_pool {

template <class T>
struct ObjectPoolDeleter {
  void operator()(T* p) const noexcept;
};

// For classes that's both ref-counted and pooled, inheriting from this class
// can be handy (so that you don't need to write your own `RefTraits`.).
//
// Note that reference count is always initialized to one, either after
// construction or returned by object pool. So use `adopt_ptr` should you
// want to construct a `RefPtr` from a raw pointer.
template <class T>
using RefCounted = flare::RefCounted<T, ObjectPoolDeleter<T>>;

// Interface of `flare::object_pool::Get` does not align very well with
// `RefPtr`. It returns a `PooledPtr`, which itself is a RAII wrapper. To
// simplify the use of pooled `RefCounted`, we provide this method.
template <class T,
          class = std::enable_if_t<std::is_base_of_v<RefCounted<T>, T>>>
RefPtr<T> GetRefCounted() {
#ifndef NDEBUG
  auto ptr = RefPtr(adopt_ptr, Get<T>().Leak());
  FLARE_DCHECK_EQ(1, ptr->UnsafeRefCount());
  return ptr;
#else
  return RefPtr(adopt_ptr, Get<T>().Leak());  // Copy ellision.
#endif
}

template <class T>
void ObjectPoolDeleter<T>::operator()(T* p) const noexcept {
  FLARE_DCHECK_EQ(p->ref_count_.load(std::memory_order_relaxed), 0);

  // Keep ref-count as 1 for reuse.
  //
  // It shouldn't be necessary to enforce memory ordering here as any ordering
  // requirement should already been satisfied by `RefCounted<T>::Deref()`.
  p->ref_count_.store(1, std::memory_order_relaxed);
  object_pool::Put<T>(p);
}

}  // namespace flare::object_pool

#endif  // FLARE_BASE_OBJECT_POOL_REF_COUNTED_H_
