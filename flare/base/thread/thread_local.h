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

#ifndef FLARE_BASE_THREAD_THREAD_LOCAL_H_
#define FLARE_BASE_THREAD_THREAD_LOCAL_H_

#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

#include "flare/base/function.h"
#include "flare/base/logging.h"
#include "flare/base/thread/thread_local/always_initialized.h"
#include "flare/base/thread/thread_local/detail/atomic_ptr.h"

namespace flare {

// Performance note:
//
// In some versions of Google's gperftools (tcmalloc), allocating memory from
// different threads often results in adjacent addresses (within a cacheline
// boundary). This allocation scheme CAN EASILY LEAD TO FALSE-SHARING AND HURT
// PERFORMANCE. As we often use `ThreadLocal<T>` for perf. optimization, this (I
// would say it's a bug) totally defeat the reason why we want a "thread-local"
// copy in the first place.
//
// Due to technical reasons (we don't have control on how instances of `T` are
// allocated), we can't workaround this quirk for you automatically, you need to
// annotate your `T` with `alignas` yourself.
//
// TODO(luobogao): It the default constructor of `ThreadLocal<T>` is used, we
// indeed can do alignment for the user.

// Support thread-local storage, with extra capability to traverse all instances
// among threads.
//
// IT'S EXPLICITLY NOT SUPPORTED TO CONSTRUCT / DESTROY OTHER THREAD-LOCAL
// VARIABLES IN CONSTRUCTOR / DESTRUCTOR OF THIS CLASS.
template <class T>
class ThreadLocal {
 public:
  constexpr ThreadLocal()
      : ThreadLocal([]() { return std::make_unique<T>(); }) {}

  template <
      class F,
      std::enable_if_t<std::is_invocable_r_v<std::unique_ptr<T>, F>>* = nullptr>
  explicit ThreadLocal(F&& creator) : creator_(std::forward<F>(creator)) {}

  T* Get() const {
    // NOT locked, I'm not sure if this is right. (However, it nonetheless
    // should work without problem and nobody else should make it non-null.)
    auto&& ptr = raw_tls_->Get();
    return FLARE_LIKELY(!!ptr) ? ptr : GetSlow();
  }
  T* operator->() const { return Get(); }
  T& operator*() const { return *Get(); }

  T* Leak() noexcept {
    std::scoped_lock _(init_lock_);
    return raw_tls_->Leak();
  }

  void Reset(std::unique_ptr<T> ptr = nullptr) noexcept {
    std::scoped_lock _(init_lock_);
    raw_tls_->Set(std::move(ptr));
  }

  // `ForEach` calls `f` with pointer (i.e. `T*`) to each thread-local
  // instances.
  //
  // CAUTION: Called with internal lock held. You may not touch TLS in `f`.
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
  ThreadLocal(const ThreadLocal&) = delete;
  ThreadLocal& operator=(const ThreadLocal&) = delete;

 private:
  [[gnu::noinline]] T* GetSlow() const {
    std::scoped_lock _(init_lock_);
    raw_tls_->Set(creator_());
    return raw_tls_->Get();
  }

  mutable internal::ThreadLocalAlwaysInitialized<
      tls::detail::AtomicScopedPtr<T>>
      raw_tls_;

  // Synchronizes between `ForEach` and other member methods operating on
  // `raw_tls_`.
  mutable std::mutex init_lock_;
  Function<std::unique_ptr<T>()> creator_;
};

}  // namespace flare

#endif  // FLARE_BASE_THREAD_THREAD_LOCAL_H_
