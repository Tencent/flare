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

#ifndef FLARE_BASE_THREAD_THREAD_LOCAL_REF_COUNTED_H_
#define FLARE_BASE_THREAD_THREAD_LOCAL_REF_COUNTED_H_

#include <type_traits>
#include <utility>

#include "flare/base/ref_ptr.h"
#include "flare/base/thread/thread_local/always_initialized.h"
#include "flare/base/thread/thread_local/detail/atomic_ptr.h"

namespace flare::internal {

// Performance note: The one on `ThreadLocal` applies here as well.

// Almost the same as `ThreadLocal` except that this one internally uses a
// `RefPtr` to keep the actual object.
//
// This can be rather weird, as it's possible for thread-local objects to live
// after its owner thread's life.
//
// Due to its hard-to-reason-about nature, this class is RESERVED FOR INTERNAL
// USE.
//
// Some utilities in flare need this class to defer processing of thread-local
// variables to background thread (when the variable's owner thread may have
// gone.).
//
// IT'S EXPLICITLY NOT SUPPORTED TO CONSTRUCT / DESTROY OTHER THREAD-LOCAL
// VARIABLES IN CONSTRUCTOR / DESTRUCTOR OF THIS CLASS.
template <class T>
class ThreadLocalRefCounted {
 public:
  constexpr ThreadLocalRefCounted()
      : ThreadLocalRefCounted([]() { return MakeRefCounted<T>(); }) {}

  template <class F,
            std::enable_if_t<std::is_invocable_r_v<RefPtr<T>, F>>* = nullptr>
  explicit ThreadLocalRefCounted(F&& creator)
      : creator_(std::forward<F>(creator)) {}

  // Accessor.
  T* Get() const {
    if (auto ptr = raw_tls_->Get()) {
      return ptr;
    }
    return GetSlow();
  }
  T* operator->() const { return Get(); }
  T& operator*() const { return *Get(); }

  // Release ownership.
  T* Leak() noexcept {
    std::scoped_lock _(init_lock_);
    return raw_tls_->Leak();
  }

  // Reset internal pointer.
  void Reset(RefPtr<T> p = nullptr) noexcept {
    std::scoped_lock _(init_lock_);
    raw_tls_->Set(p);
  }

  // Traversal thread-local objects.
  template <class F>
  void ForEach(F&& f) const {
    std::scoped_lock _(init_lock_);
    raw_tls_.ForEach([&](auto* p) {
      if (auto ptr = p->Get()) {
        f(ptr);
      }
    });
  }

  // Noncopyable, nonmovable.
  ThreadLocalRefCounted(const ThreadLocalRefCounted&) = delete;
  ThreadLocalRefCounted& operator=(const ThreadLocalRefCounted&) = delete;

 private:
  [[gnu::noinline]] T* GetSlow() const {
    std::scoped_lock _(init_lock_);
    raw_tls_->Set(creator_());
    return raw_tls_->Get();
  }

 private:
  mutable internal::ThreadLocalAlwaysInitialized<tls::detail::AtomicRefPtr<T>>
      raw_tls_;

  mutable std::mutex init_lock_;
  Function<RefPtr<T>()> creator_;
};

}  // namespace flare::internal

#endif  // FLARE_BASE_THREAD_THREAD_LOCAL_REF_COUNTED_H_
