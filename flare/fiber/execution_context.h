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

#ifndef FLARE_FIBER_EXECUTION_CONTEXT_H_
#define FLARE_FIBER_EXECUTION_CONTEXT_H_

#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "flare/base/deferred.h"
#include "flare/base/internal/index_alloc.h"
#include "flare/base/object_pool/ref_counted.h"
#include "flare/fiber/fiber_local.h"

namespace flare::fiber {

namespace detail {

struct ExecutionLocalIndexTag;

}  // namespace detail

// `ExecutionContext` serves as a container for all information relevant to a
// logical fiber / a group of fibers of execution.
//
// The fiber runtime implicitly passes execution context in:
//
// - `Async`
// - `Set(Detached)Timer`
//
// Note that starting a new fiber won't automatically inheriting current
// execution context, you need to `Capture` and `Run` in it manually.
class ExecutionContext : public object_pool::RefCounted<ExecutionContext> {
 public:
  // Call `cb` in this execution context.
  template <class F>
  decltype(auto) Execute(F&& cb) {
    ScopedDeferred _([old = *current_] { *current_ = old; });
    *current_ = this;
    return std::forward<F>(cb)();
  }

  // Clear this execution context for reuse.
  void Clear();

  // Capture current execution context.
  static RefPtr<ExecutionContext> Capture();

  // Create a new execution context.
  static RefPtr<ExecutionContext> Create();

  // Get current execution context.
  static ExecutionContext* Current() { return *current_; }

 private:
  template <class>
  friend class ExecutionLocal;

  // Keep size of this structure a power of two helps code-gen.
  struct ElsEntry {
    std::atomic<void*> ptr{nullptr};
    void (*deleter)(void*);

    ~ElsEntry() {
      if (auto p = ptr.load(std::memory_order_acquire)) {
        deleter(p);
        deleter = nullptr;
        ptr.store(nullptr, std::memory_order_relaxed);
      }
    }
  };

  // For the moment we do not make heavy use of execution local storage, 8
  // should be sufficient.
  static constexpr auto kInlineElsSlots = 8;
  static FiberLocal<ExecutionContext*> current_;

  ElsEntry* GetElsEntry(std::size_t slot_index) {
    if (FLARE_LIKELY(slot_index < std::size(inline_els_))) {
      return &inline_els_[slot_index];
    }
    return GetElsEntrySlow(slot_index);
  }

  ElsEntry* GetElsEntrySlow(std::size_t slow_index);

 private:
  ElsEntry inline_els_[kInlineElsSlots];
  std::mutex external_els_lock_;
  std::unordered_map<std::size_t, ElsEntry> external_els_;

  // Lock shared by ELS initialization. Unless the execution context is
  // concurrently running in multiple threads and are all trying to initializing
  // ELS, this lock shouldn't contend too much.
  std::mutex els_init_lock_;
};

// Local storage a given execution context.
//
// Note that since execution context can be shared by multiple (possibly
// concurrently running) fibers, access to `T` must be synchronized.
//
// `ExecutionLocal` guarantees thread-safety when initialize `T`.
template <class T>
class ExecutionLocal {
 public:
  ExecutionLocal() : slot_index_(GetIndexAlloc()->Next()) {}
  ~ExecutionLocal() { GetIndexAlloc()->Free(slot_index_); }

  // Accessor.
  T* operator->() const noexcept { return get(); }
  T& operator*() const noexcept { return *get(); }
  T* get() const noexcept { return Get(); }

  // Initializes the value (in a single-threaded env., as obvious). This can
  // save you the overhead of grabbing initialization lock. Beside, this allows
  // you to specify your own deleter.
  //
  // This method is provided for perf. reasons, and for the moment it's FOR
  // INTERNAL USE ONLY.
  void UnsafeInit(T* ptr, void (*deleter)(T*)) {
    auto&& entry = ExecutionContext::Current()->GetElsEntry(slot_index_);
    FLARE_CHECK(entry,
                "Initializing ELS must be done inside execution context.");
    FLARE_CHECK(entry->ptr.load(std::memory_order_relaxed) == nullptr,
                "Initializeing an already-initialized ELS?");
    entry->ptr.store(ptr, std::memory_order_release);
    // FIXME: U.B. here?
    entry->deleter = reinterpret_cast<void (*)(void*)>(deleter);
  }

 private:
  T* Get() const noexcept {
    auto&& current = ExecutionContext::Current();
    FLARE_DCHECK(current,
                 "Getting ELS is only meaningful inside execution context.");

    auto&& entry = current->GetElsEntry(slot_index_);
    if (auto ptr = entry->ptr.load(std::memory_order_acquire);
        FLARE_LIKELY(ptr)) {
      return reinterpret_cast<T*>(ptr);  // Already initialized. life is good.
    }
    return UninitializedGetSlow();
  }

  T* UninitializedGetSlow() const noexcept;

  static flare::internal::IndexAlloc* GetIndexAlloc() noexcept {
    return flare::internal::IndexAlloc::For<detail::ExecutionLocalIndexTag>();
  }

 private:
  std::size_t slot_index_;
};

// Calls `f`, possibly within an execution context, if one is given.
template <class F>
void WithExecutionContextIfPresent(ExecutionContext* ec, F&& f) {
  if (ec) {
    ec->Execute(std::forward<F>(f));
  } else {
    std::forward<F>(f)();
  }
}

////////////////////////////////////////
// Implementation goes below.         //
////////////////////////////////////////

template <class T>
T* ExecutionLocal<T>::UninitializedGetSlow() const noexcept {
  auto ectx = ExecutionContext::Current();
  auto&& entry = ectx->GetElsEntry(slot_index_);
  std::scoped_lock _(ectx->els_init_lock_);
  if (entry->ptr.load(std::memory_order_acquire) == nullptr) {  // DCLP.
    auto ptr = std::make_unique<T>();
    entry->deleter = [](auto* p) { delete reinterpret_cast<T*>(p); };
    entry->ptr.store(ptr.release(), std::memory_order_release);
  }

  // Memory order does not matter here, the object is visible to us anyway.
  return reinterpret_cast<T*>(entry->ptr.load(std::memory_order_relaxed));
}

}  // namespace flare::fiber

namespace flare {

template <>
struct PoolTraits<fiber::ExecutionContext> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 8192;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 1024;
  static constexpr auto kTransferBatchSize = 1024;

  // Free any resources held by `ec` prior to recycling it.
  static void OnPut(fiber::ExecutionContext* ec) { ec->Clear(); }
};

}  // namespace flare

#endif  // FLARE_FIBER_EXECUTION_CONTEXT_H_
