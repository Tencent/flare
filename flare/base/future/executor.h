// Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_BASE_FUTURE_EXECUTOR_H_
#define FLARE_BASE_FUTURE_EXECUTOR_H_

#include <atomic>
#include <memory>
#include <utility>

#include "flare/base/function.h"

namespace flare::future {

// This class is a lightweight polymorphic wrapper for execution contexts,
// which are responsible for executing jobs posted to them.
//
// Both this wrapper and the wrappee is (required) to be CopyConstructible.
//
// Our design follows how p0443r7 declares `std::executor`: Exactly the same
// way as how `std::function` wraps `Callable`.
//
// This design suffers from slightly bad copy performance (if it's critical
// at all), but provide us (as well as the users) the advantages that all
// type erasure designs enjoy over inheritance: Non-intrusive, better lifetime
// management, easy-to-reason-about interface, etc.
//
// @sa: https://github.com/executors/executors/blob/master/explanatory.md
class Executor {
 public:
  // DefaultConstructible. May be used as placeholder.
  Executor() = default;

  // Copy & move.
  Executor(const Executor& e) : impl_(e.impl_->Clone()) {}
  Executor(Executor&&) = default;
  Executor& operator=(const Executor& e) {
    impl_ = e.impl_->Clone();
    return *this;
  }
  Executor& operator=(Executor&&) = default;

  // Wraps a real executor in `this`.
  //
  // `T::Execute([]{})` must be a valid expression for this overload
  // to participate in resolution.
  template <
      class T,
      class = std::enable_if_t<!std::is_same_v<Executor, std::decay_t<T>>>,
      class = decltype(std::declval<T&&>().Execute(Function<void()>()))>
  /* implicit */ Executor(T&& executor) {
    impl_ = EraseExecutorType(std::forward<T>(executor));
  }

  // It's allowed (but not required, and generally discouraged) to invoke
  // `job` immediately, before returning to the caller.
  template <class T>
  void Execute(T&& job) {
    impl_->Execute(std::forward<T>(job));
  }

 private:
  // Type erased `Executor`s (concept here).
  class ConcreteExecutor {
   public:
    virtual ~ConcreteExecutor() = default;

    // It should be possible to declare the argument as a customized
    // non-moving wrapper behaves like `FunctionView` but provide the
    // following in addition:
    //
    //  - It requires the functor to be MoveConstructible.
    //  - It's possible to move-construct a `Function<void()>` from the
    //    wrapper when needed.
    //
    // By doing this, `ConcreteExecutor` itself does not have to require
    // a move. This can be helpful for things like `InlineExecutor`.
    //
    // In case it's indeed desired, the derived class could move construct
    // a `Function<...>` itself.
    //
    // For now we don't do this, as there's little sense in using an
    // InlineExecutor too much. For the asynchronous executors, a move is
    // inevitable.
    virtual void Execute(Function<void()> job) = 0;

    // Clone the executor.
    //
    // Here we requires the `Executor` to be CopyConstructible.
    virtual std::unique_ptr<ConcreteExecutor> Clone() = 0;
  };

  // This class holds all the information we need to call a concrete
  // `Executor` (concept here).
  //
  // `Executor` is copy / moved into `*this`. It's should be relatively
  // cheap anyway.
  template <class T>
  class ConcreteExecutorImpl : public ConcreteExecutor {
   public:
    template <class U>
    explicit ConcreteExecutorImpl(U&& e) : impl_(std::forward<U>(e)) {}

    void Execute(Function<void()> job) override {
      return impl_.Execute(std::move(job));
    }

    std::unique_ptr<ConcreteExecutor> Clone() override {
      return std::make_unique<ConcreteExecutorImpl>(impl_);  // Copied here.
    }

   private:
    T impl_;
  };

  // Erase type of `executor` and copy it to some class inherited from
  // `ConcreteExecutor`.
  template <class T>
  std::unique_ptr<ConcreteExecutor> EraseExecutorType(T&& executor) {
    return std::make_unique<ConcreteExecutorImpl<std::decay_t<T>>>(
        std::forward<T>(executor));
  }

  // Type-erased wrapper for `Executor`s.
  std::unique_ptr<ConcreteExecutor> impl_;
};

// An "inline" executor just invokes the jobs posted to it immediately.
//
// Be careful not to overflow the stack if you're calling `Execute` in `job`.
class InlineExecutor {
 public:
  void Execute(Function<void()> job) { job(); }  // Too simple, sometimes naive.
};

namespace detail {

// Default executor to use.
//
// Setting a new default executor won't affect `Future`s already
// constructed, nor will it affects the `Future`s from `Future.Then`.
//
// Only `Future`s returned by newly constructed `Promise`'s `GetFuture`
// will respect the new settings.
//
// Declared as a standalone method rather than a global variable to
// mitigate issues with initialization order.
inline std::shared_ptr<Executor>& DefaultExecutorPtr() {
  static std::shared_ptr<Executor> ptr = std::make_shared<Executor>(
      InlineExecutor());  // The default one is a bad one, indeed.
  return ptr;
}

}  // namespace detail

// Get the current default executor.
inline Executor GetDefaultExecutor() {
  // I haven't check the implementation yet but even if this is lock-free,
  // copying a `std::shared_ptr<...>` is effectively a global ref-counter,
  // which scales bad.
  //
  // This shouldn't hurt, though. We're only called when a new `Promise` is
  // made, which is relatively rare. Calling `Then` on `Future` won't result
  // in a call to us.
  return *std::atomic_load_explicit(&detail::DefaultExecutorPtr(),
                                    std::memory_order_acquire);  // Copied here.
}

// Set the default executor to use.
//
// The old executor is returned.
inline Executor SetDefaultExecutor(Executor exec) {
  auto ptr = std::make_shared<Executor>(std::move(exec));

  // Do NOT `std::move` here, even if the pointer is going to be destroyed.
  // The pointee (not just the pointer) may well be possibly used by others
  // concurrently.
  //
  // Copying the executor should be thread-safe, anyway.
  return *std::atomic_exchange_explicit(
      &detail::DefaultExecutorPtr(), std::move(ptr), std::memory_order_acq_rel);
}

}  // namespace flare::future

#endif  // FLARE_BASE_FUTURE_EXECUTOR_H_
