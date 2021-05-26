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

#ifndef FLARE_BASE_THREAD_THREAD_LOCAL_ALWAYS_INITIALIZED_H_
#define FLARE_BASE_THREAD_THREAD_LOCAL_ALWAYS_INITIALIZED_H_

#include <utility>

#include "flare/base/function.h"
#include "flare/base/thread/thread_local/object_array.h"

namespace flare::internal {

// Same as `ThreadLocal<T>` except that objects are initialized eagerly (also
// nondeterministically.). Note that `T`'s constructor may not touch other TLS
// variables, otherwise the behavior is undefined.
//
// Performs slightly better. For internal use only.
//
// Instances of `T` in different threads are guaranteed to reside in different
// cache line. However, if `T` itself allocates memory, there's no guarantee on
// how memory referred by `T` in different threads are allocated.
//
// IT'S EXPLICITLY NOT SUPPORTED TO CONSTRUCT / DESTROY OTHER THREAD-LOCAL
// VARIABLES IN CONSTRUCTOR / DESTRUCTOR OF THIS CLASS.
//
// TODO(luobogao): Let's refine its name and move it out for public use.
template <class T>
class ThreadLocalAlwaysInitialized {
 public:
  ThreadLocalAlwaysInitialized();
  ~ThreadLocalAlwaysInitialized();

  // Initialize object with a customized initializer.
  template <class F>
  explicit ThreadLocalAlwaysInitialized(F&& initializer);

  // Noncopyable / nonmovable.
  ThreadLocalAlwaysInitialized(const ThreadLocalAlwaysInitialized&) = delete;
  ThreadLocalAlwaysInitialized& operator=(const ThreadLocalAlwaysInitialized&) =
      delete;

  // Accessors.
  T* Get() const;
  T* operator->() const { return Get(); }
  T& operator*() const { return *Get(); }

  // Traverse through all instances among threads.
  //
  // CAUTION: Called with internal lock held. You may not touch TLS in `f`.
  // (Maybe we should name it as `ForEachUnsafe` or `ForEachLocked`?)
  template <class F>
  void ForEach(F&& f) const;

 private:
  // Placed as the first member to keep accessing it quick.
  //
  // Always a multiple of `kEntrySize`.
  std::ptrdiff_t offset_;
  Function<void(void*)> initializer_;
};

///////////////////////////////////////
// Implementation goes below.        //
///////////////////////////////////////

template <class T>
ThreadLocalAlwaysInitialized<T>::ThreadLocalAlwaysInitialized()
    : ThreadLocalAlwaysInitialized([](void* ptr) { new (ptr) T(); }) {}

template <class T>
template <class F>
ThreadLocalAlwaysInitialized<T>::ThreadLocalAlwaysInitialized(F&& initializer)
    : initializer_(std::forward<F>(initializer)) {
  // Initialize the slot in every thread (that has allocated the slot in its
  // thread-local object array.).
  auto slot_initializer = [&](auto index) {
    // Called with layout lock held. Nobody else should be resizing its own
    // object array or mutating the (type-specific) global layout.

    // Initialize all slots (if the slot itself has already been allocated in
    // corresponding thread's object array) immediately so that we don't need to
    // check for initialization on `Get()`.
    tls::detail::ObjectArrayRegistry<T>::Instance()->BroadcastingForEachLocked(
        index, [&](auto* p) { p->objects.InitializeAt(index, initializer_); });
  };

  // Allocate a slot.
  offset_ = tls::detail::ObjectArrayLayout<T>::Instance()->CreateEntry(
                &initializer_, slot_initializer) *
            sizeof(T);
}

template <class T>
ThreadLocalAlwaysInitialized<T>::~ThreadLocalAlwaysInitialized() {
  FLARE_CHECK_EQ(offset_ % sizeof(T), 0);
  auto index = offset_ / sizeof(T);

  // The slot is freed after we have destroyed all instances.
  tls::detail::ObjectArrayLayout<T>::Instance()->FreeEntry(index, [&] {
    // Called with layout lock held.

    // Destory all instances. (We actually reconstructed a new one at the place
    // we were at.)
    tls::detail::ObjectArrayRegistry<T>::Instance()->BroadcastingForEachLocked(
        index, [&](auto* p) { p->objects.DestroyAt(index); });
  });
}

template <class T>
inline T* ThreadLocalAlwaysInitialized<T>::Get() const {
  return tls::detail::GetLocalObjectArrayAt<T>(offset_);
}

template <class T>
template <class F>
void ThreadLocalAlwaysInitialized<T>::ForEach(F&& f) const {
  FLARE_CHECK_EQ(offset_ % sizeof(T), 0);
  auto index = offset_ / sizeof(T);

  tls::detail::ObjectArrayRegistry<T>::Instance()->ForEachLocked(
      index, [&](auto* p) { f(p->objects.GetAt(index)); });
}

}  // namespace flare::internal

#endif  // FLARE_BASE_THREAD_THREAD_LOCAL_ALWAYS_INITIALIZED_H_
