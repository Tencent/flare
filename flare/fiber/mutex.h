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

#ifndef FLARE_FIBER_MUTEX_H_
#define FLARE_FIBER_MUTEX_H_

#include "flare/fiber/detail/waitable.h"

namespace flare::fiber {

// Analogous to `std::mutex`, but it's for fiber.
using Mutex = detail::Mutex;

}  // namespace flare::fiber

#endif  // FLARE_FIBER_MUTEX_H_
