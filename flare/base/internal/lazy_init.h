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

#ifndef FLARE_BASE_INTERNAL_LAZY_INIT_H_
#define FLARE_BASE_INTERNAL_LAZY_INIT_H_

#include "flare/base/never_destroyed.h"

namespace flare::internal {

// In most case you WON'T need this, function-local static should work well.
// This is the reason why this method is put in `internal::`.
//
// However, for our implementation's sake, `LazyInit<T>()` could be handy in
// defining objects that could (or should) be shared globally. (For
// deterministic initialization, simplification of code, or whatever else
// reasons.)
//
// Note that instance returned by this method is NEVER destroyed.
template <class T, class Tag = void>
T* LazyInit() {
  static NeverDestroyed<T> t;
  return t.Get();
}

// The instance returned by this method is NOT the same instance as of returned
// by `LazyInit`.
//
// This method can be especially useful when you want to workaround [this issue
// (using in-class initialization of NSDM and default argument at the same
// time)](https://stackoverflow.com/a/17436088).
//
// Note that instance returned by this method is NEVER destroyed.
template <class T, class Tag = void>
const T& LazyInitConstant() {
  static NeverDestroyed<T> t;
  return *t;
}

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_LAZY_INIT_H_
