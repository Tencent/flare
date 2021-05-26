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

#ifndef FLARE_BASE_INTERNAL_EARLY_INIT_H_
#define FLARE_BASE_INTERNAL_EARLY_INIT_H_

#include "flare/base/never_destroyed.h"

namespace flare::internal {

namespace detail {

// CAUTION: Global object initialization order fiasco.
template <class T, class Tag = void>
struct EarlyInitInstance {
  inline static NeverDestroyed<T> object;
};

}  // namespace detail

// Returns a const ref to `T` instance.
//
// This method can be handy for initializing default arguments.
//
// For better code-gen, this method can be used when necessary. However, unlike
// its counterpart in `lazy_init.h`, here you risk global object initialization
// order fiasco.
//
// Note that instance returned by this method is NEVER destroyed.
template <class T, class Tag = void>
const T& EarlyInitConstant() {
  return *detail::EarlyInitInstance<T, Tag>::object.Get();
}

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_EARLY_INIT_H_
