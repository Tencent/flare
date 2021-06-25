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

#ifndef FLARE_BASE_DEFERRED_H_
#define FLARE_BASE_DEFERRED_H_

#include <utility>

#include "flare/base/function.h"

namespace flare {

// Call `F` on destruction.
template <class F>
class ScopedDeferred {
 public:
  explicit ScopedDeferred(F&& f) : action_(std::move(f)) {}
  ~ScopedDeferred() { action_(); }

  // Noncopyable / nonmovable.
  ScopedDeferred(const ScopedDeferred&) = delete;
  ScopedDeferred& operator=(const ScopedDeferred&) = delete;

 private:
  F action_;
};

// Call action on destruction. Moveable. Dismissable.
class Deferred {
 public:
  Deferred() = default;

  template <class F>
  explicit Deferred(F&& f) : action_(std::forward<F>(f)) {}
  Deferred(Deferred&& other) noexcept { action_ = std::move(other.action_); }
  Deferred& operator=(Deferred&& other) noexcept {
    if (&other == this) {
      return *this;
    }
    Fire();
    action_ = std::move(other.action_);
    return *this;
  }
  ~Deferred() {
    if (action_) {
      action_();
    }
  }

  explicit operator bool() const noexcept { return !!action_; }

  void Fire() noexcept {
    if (auto op = std::move(action_)) {
      op();
    }
  }

  void Dismiss() noexcept { action_ = nullptr; }

 private:
  Function<void()> action_;
};

// Do we need a `FLARE_DEFER`?

}  // namespace flare

#endif  // FLARE_BASE_DEFERRED_H_
