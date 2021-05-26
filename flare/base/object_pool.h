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

#ifndef FLARE_BASE_OBJECT_POOL_H_
#define FLARE_BASE_OBJECT_POOL_H_

#include <chrono>
#include <mutex>
#include <utility>

#include "flare/base/likely.h"
#include "flare/base/logging.h"
#include "flare/base/object_pool/disabled.h"
#include "flare/base/object_pool/global.h"
#include "flare/base/object_pool/memory_node_shared.h"
#include "flare/base/object_pool/thread_local.h"

namespace flare {

// For the moment, only `MemoryNodeShared` is highly optimized, and it likely
// will outperform all other type of pools.
enum class PoolType {
  // Do not use object pool at all.
  //
  // This type is normally used for debugging purpose. (Object pooling makes it
  // hard to tracing object creation, by disabling it, debugging can be easier.)
  Disabled,

  // Cache objects in a thread local cache.
  //
  // This type has the highest performance if your object allocation /
  // deallocation is done evenly in every thread.
  //
  // No lock / synchronization is required for this type of pool
  ThreadLocal,

  // Cache a small amount of objects locally, and use a shared pool for threads
  // in the same NUMA Node.
  //
  // If your objects is allocated in one thread, but freed in other threads in
  // the same scheduling group. This type of pool might work better.
  MemoryNodeShared,

  // Cache a small amount of objects locally, and the rest are cached in a
  // global pool.
  //
  // This type of pool might work not-as-good as the above ones, but if your
  // workload has no evident allocation / deallocation pattern, this type might
  // suit most.
  Global
};

// Note that this pool uses thread-local cache. That is, it does not perform
// well in scenarios such as producer-consumer (in this case, the producer
// thread keeps allocating objects while the consumer thread keeps de-allocating
// objects, and nothing could be reused by either thread.). Be aware of this.

// You need to customize these parameters before using this object pool.
template <class T>
struct PoolTraits {
  // Type of backend pool to be used for this type. Check comments in `PoolType`
  // for their explanation.
  //
  // static constexpr PoolType kType = ...;  // Or `kPoolType`?

  // If your type cannot be created by `new T()`, you can provide a factory
  // here.
  //
  // Leave it undefined if you don't need it.
  //
  // static T* Create() { ... }

  // If you type cannot be destroyed by `delete ptr`, you can provide a
  // customized deleter here.
  //
  // Leave it undefined if you don't need it.
  //
  // void Destroy(T* ptr) { ... }

  // Below are several hooks.
  //
  // For those hooks you don't need, leave them as not defined.

  // Hook for `Get`. It's called after an object is retrieved from the pool.
  // This hook can be used for resetting objects to a "clean" state so that
  // users won't need to reset objects themselves.
  //
  // static void OnGet(T*) { ... }

  // Hook for `Put`. It's called before an object is put into the pool. It can
  // be handy if you want to release specific precious resources (handle to
  // temporary file, for example) before the object is hold by the pool.
  //
  // static void OnPut(T*) { ... }

  // For type-specific arguments, see header for the corresponding backend.

  // ... TODO

  static_assert(sizeof(T) == 0,
                "You need to specialize `flare::object_pool::PoolTraits` to "
                "specify parameters before using `object_pool::Xxx`.");
};

namespace object_pool::detail {

// Call corresponding backend to get an object. Hook is not called.
template <class T>
inline void* GetWithoutHook() {
  constexpr auto kType = PoolTraits<T>::kType;

  if constexpr (kType == PoolType::Disabled) {
    return disabled::Get(*GetTypeDesc<T>());
  } else if constexpr (kType == PoolType::ThreadLocal) {
    return tls::Get(*GetTypeDesc<T>(), tls::GetThreadLocalPool<T>());
  } else if constexpr (kType == PoolType::MemoryNodeShared) {
    return memory_node_shared::Get<T>();
  } else if constexpr (kType == PoolType::Global) {
    return global::Get(*GetTypeDesc<T>());
  } else {
    static_assert(sizeof(T) == 0, "Unexpected pool type.");
    FLARE_CHECK(0);
  }
}

// Call corresponding backend to return an object. Hook is called by the caller.
template <class T>
inline void PutWithoutHook(void* ptr) {
  constexpr auto kType = PoolTraits<T>::kType;

  if constexpr (kType == PoolType::Disabled) {
    disabled::Put(*GetTypeDesc<T>(), ptr);
  } else if constexpr (kType == PoolType::ThreadLocal) {
    tls::Put(*GetTypeDesc<T>(), tls::GetThreadLocalPool<T>(), ptr);
  } else if constexpr (kType == PoolType::MemoryNodeShared) {
    return memory_node_shared::Put<T>(ptr);
  } else if constexpr (kType == PoolType::Global) {
    global::Put(*GetTypeDesc<T>(), ptr);
  } else {
    static_assert(sizeof(T) == 0, "Unexpected pool type.");
    FLARE_CHECK(0);
  }
}

// Get an object from the corresponding backend.
template <class T>
inline void* Get() {
  auto rc = GetWithoutHook<T>();
  OnGetHook<T>(rc);
  return rc;
}

// Put an object to the corresponding backend.
template <class T>
inline void Put(void* ptr) {
  FLARE_DCHECK(ptr,
               "I'm pretty sure null pointer is not what you got when you "
               "called `Get`.");
  OnPutHook<T>(ptr);
  return PutWithoutHook<T>(ptr);
}

}  // namespace object_pool::detail

// RAII wrapper for resources allocated from object pool.
template <class T>
class PooledPtr final {
 public:
  constexpr PooledPtr();
  ~PooledPtr();

  /* implicit */ constexpr PooledPtr(std::nullptr_t) : PooledPtr() {}

  // Used by `Get<T>()`. You don't want to call this normally.
  constexpr explicit PooledPtr(T* ptr) : ptr_(ptr) {}

  // Movable but not copyable.
  constexpr PooledPtr(PooledPtr&& ptr) noexcept;
  constexpr PooledPtr& operator=(PooledPtr&& ptr) noexcept;

  // Test if the pointer is nullptr.
  constexpr explicit operator bool() const noexcept;

  // Accessor.
  constexpr T* Get() const noexcept;
  constexpr T* operator->() const noexcept;
  constexpr T& operator*() const noexcept;

  // Equivalent to `Reset(nullptr)`.
  constexpr PooledPtr& operator=(std::nullptr_t) noexcept;

  // `ptr` must be obtained from calling `Leak()` on another `PooledPtr`.
  constexpr void Reset(T* ptr = nullptr) noexcept;

  // Ownership is transferred to caller.
  [[nodiscard]] constexpr T* Leak() noexcept;

 private:
  T* ptr_;
};

namespace object_pool {

// Acquire an object.
template <class T>
PooledPtr<T> Get() {
  return PooledPtr<T>(static_cast<T*>(detail::Get<T>()));
}

// Release an object that was previously acquired by `Get()` and subsequently
// leaked from `PooledPtr::Leak()`.
//
// Note that unless you explicitly leaked a pointer from `PooledPtr`, you won't
// need to call this explicitly.
template <class T>
void Put(std::common_type_t<T>* ptr) {
  return detail::Put<T>(ptr);
}

namespace internal {

// Initialize object pool for this thread.
//
// Ususally this is done automatically on the first time you call object pool
// API. However, in certain cases you may want to initialize object pool early.
// This method provides a *limited* way to accomplish this.
//
// Note that this method does NOT guarantee that the object pool is fully
// initialized. It's only a hint to implementation backends.
//
// Calling this method for multiple times is explicitly allowed.
void InitializeObjectPoolForCurrentThread();

}  // namespace internal

}  // namespace object_pool

template <class T>
constexpr PooledPtr<T>::PooledPtr() : ptr_(nullptr) {}

template <class T>
PooledPtr<T>::~PooledPtr() {
  if (ptr_) {
    object_pool::Put<T>(ptr_);
  }
}

template <class T>
constexpr PooledPtr<T>::PooledPtr(PooledPtr&& ptr) noexcept : ptr_(ptr.ptr_) {
  ptr.ptr_ = nullptr;
}

template <class T>
constexpr PooledPtr<T>& PooledPtr<T>::operator=(PooledPtr&& ptr) noexcept {
  if (FLARE_LIKELY(this != &ptr)) {
    Reset(ptr.Leak());
  }
  return *this;
}

template <class T>
constexpr PooledPtr<T>::operator bool() const noexcept {
  return !!ptr_;
}

template <class T>
constexpr T* PooledPtr<T>::Get() const noexcept {
  return ptr_;
}

template <class T>
constexpr T* PooledPtr<T>::operator->() const noexcept {
  return ptr_;
}

template <class T>
constexpr T& PooledPtr<T>::operator*() const noexcept {
  return *ptr_;
}

template <class T>
constexpr PooledPtr<T>& PooledPtr<T>::operator=(std::nullptr_t) noexcept {
  Reset(nullptr);
  return *this;
}

template <class T>
constexpr void PooledPtr<T>::Reset(T* ptr) noexcept {
  if (ptr_) {
    object_pool::Put<T>(ptr_);
  }
  ptr_ = ptr;
}

template <class T>
constexpr T* PooledPtr<T>::Leak() noexcept {
  return std::exchange(ptr_, nullptr);
}

template <class T>
constexpr bool operator==(const PooledPtr<T>& ptr, std::nullptr_t) {
  return ptr.Get() == nullptr;
}

template <class T>
constexpr bool operator==(std::nullptr_t, const PooledPtr<T>& ptr) {
  return ptr.Get() == nullptr;
}

// TODO(luobogao): Other comparison operators.

}  // namespace flare

#endif  // FLARE_BASE_OBJECT_POOL_H_
