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

#ifndef FLARE_FIBER_DETAIL_RUNNABLE_ENTITY_H_
#define FLARE_FIBER_DETAIL_RUNNABLE_ENTITY_H_

#include "flare/base/casting.h"

namespace flare::fiber::detail {

// Base class for all runnable entities (those recognized by `RunQueue`).
//
// Use `isa<T>` or `dyn_cast<T>` to test its type and convert it to its
// subclass.
//
// `SchedulingGroup` is responsible for converting this object to `FiberEntity`,
// regardless of its current type.
//
// Indeed we can make things more flexible by introducing `virtual void Run()`
// to this base class. That way we can even accept callbacks and behaves like a
// thread pool if necessary. However, I don't see the need for that flexibility
// to warrant its cost.
struct RunnableEntity : ExactMatchCastable {};

}  // namespace flare::fiber::detail

#endif  // FLARE_FIBER_DETAIL_RUNNABLE_ENTITY_H_
