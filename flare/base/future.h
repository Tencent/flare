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

#ifndef FLARE_BASE_FUTURE_H_
#define FLARE_BASE_FUTURE_H_

#include "flare/base/future/future-impl.h"
#include "flare/base/future/future.h"
#include "flare/base/future/promise-impl.h"
#include "flare/base/future/promise.h"
#include "flare/base/future/utils.h"

namespace flare {

using future::Future;
using future::futurize_tuple;
using future::futurize_values;
using future::MakeReadyFuture;
using future::Promise;
using future::Split;
using future::WhenAll;
using future::WhenAny;

// `flare::future::BlockingGet` is NOT imported into `flare::`.
//
// If you need to wait for asynchronous operation in _pthread environment_,
// qualify your call to `BlockingGet` with `flare::future::`.

}  // namespace flare

#endif  // FLARE_BASE_FUTURE_H_
