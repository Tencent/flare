// Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_FIBER_ALTERNATIVES_H_
#define FLARE_FIBER_ALTERNATIVES_H_

#include <thread>

// This file provides you alternatives for accessing special variables safely
// (e.g., `errno`). By default it's unsafe to access some of them more than once
// if fiber rescheduling happens in between.

namespace flare::fiber {

// Fiber-safe alternative for reading `errno`.
//
// You don't need this method if no fiber-rescheduling happens in your method.
int GetLastError();

// Fiber-safe alternative for setting `errno`.
void SetLastError(int error);

// Reads current thread's ID.
std::thread::id GetCurrentThreadId();

}  // namespace flare::fiber

#endif  // FLARE_FIBER_ALTERNATIVES_H_
