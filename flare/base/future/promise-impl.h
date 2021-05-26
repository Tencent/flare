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

#ifndef FLARE_BASE_FUTURE_PROMISE_IMPL_H_
#define FLARE_BASE_FUTURE_PROMISE_IMPL_H_

#include "flare/base/future/promise.h"

#include <memory>
#include <utility>

#include "flare/base/future/executor.h"
#include "flare/base/future/future.h"

namespace flare::future {

template <class... Ts>
Promise<Ts...>::Promise()
    : core_(std::make_shared<Core<Ts...>>(GetDefaultExecutor())) {}

template <class... Ts>
Future<Ts...> Promise<Ts...>::GetFuture() {
  return Future<Ts...>(core_);
}

template <class... Ts>
template <class... Us, class>
void Promise<Ts...>::SetValue(Us&&... values) {
  core_->SetBoxed(Boxed<Ts...>(box_values, std::forward<Us>(values)...));
}

template <class... Ts>
void Promise<Ts...>::SetBoxed(Boxed<Ts...> boxed) {
  core_->SetBoxed(std::move(boxed));
}

template <class... Ts>
Promise<Ts...>::Promise(Executor executor)
    : core_(std::make_shared<Core<Ts...>>(std::move(executor))) {}

}  // namespace flare::future

#endif  // FLARE_BASE_FUTURE_PROMISE_IMPL_H_
