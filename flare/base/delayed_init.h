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

#ifndef FLARE_BASE_DELAYED_INIT_H_
#define FLARE_BASE_DELAYED_INIT_H_

#include <optional>
#include <utility>

#include "flare/base/internal/logging.h"

namespace flare {

// This class allows you to delay initialization of an object.
template <class T>
class DelayedInit {
 public:
  template <class... Args>
  void Init(Args&&... args) {
    value_.emplace(std::forward<Args>(args)...);
  }

  void Destroy() { value_ = std::nullopt; }

  T* operator->() {
    FLARE_DCHECK(value_);
    return &*value_;
  }

  const T* operator->() const {
    FLARE_DCHECK(value_);
    return &*value_;
  }

  T& operator*() {
    FLARE_DCHECK(value_);
    return *value_;
  }

  const T& operator*() const {
    FLARE_DCHECK(value_);
    return *value_;
  }

  explicit operator bool() const { return !!value_; }

 private:
  std::optional<T> value_;
};

}  // namespace flare

#endif  // FLARE_BASE_DELAYED_INIT_H_
