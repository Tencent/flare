// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_TESTING_DETAIL_IMPLICITLY_CASTING_H_
#define FLARE_TESTING_DETAIL_IMPLICITLY_CASTING_H_

#include "flare/base/down_cast.h"

namespace flare::testing::detail {

// This type helps you to implement `ACTION_P` if you need to down-cast
// arguments.
template <class Base>
class ImplicitlyCasting {
 public:
  explicit ImplicitlyCasting(const Base* ptr) : ptr_(ptr) {}

  template <class T> /* implicit */ operator T*() const noexcept {
    return flare::down_cast<T>(const_cast<Base*>(ptr_));
  }
  template <class T> /* implicit */ operator const T*() const noexcept {
    return flare::down_cast<T>(ptr_);
  }
  template <class T> /* implicit */ operator T&() const noexcept {
    return *static_cast<T*>(*this);
  }
  template <class T> /* implicit */ operator const T&() const noexcept {
    return *static_cast<const T*>(*this);
  }

 private:
  const Base* ptr_;
};

}  // namespace flare::testing::detail

#endif  // FLARE_TESTING_DETAIL_IMPLICITLY_CASTING_H_
