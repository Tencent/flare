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

#include "flare/fiber/alternatives.h"

#include <errno.h>

#include <thread>

namespace flare::fiber {

int GetLastError() {
  // Nothing special here.
  //
  // The reason we can't safely use `errno` in fiber environment is that the
  // compiler may cache intermediate result (usually an internal pointer to
  // actual errno variable) in register in a fiber-unsafe way, when `errno` is
  // accessed consecutively.
  //
  // By asking user to substitute `errno` with a call to `GetLastError`, the
  // compiler no longer sees the aforementioned intermediate result. Therefore
  // we no longer risk having the compiler caching it, and life is good.
  //
  // We ourselves doesn't affected by the possible register-caching either
  // (unless we get inline-d, which is not the case here) -- The below is our
  // first access to `errno`, thus no caching is possible (assuming no LTO).
  return errno;
}

void SetLastError(int error) { errno = error; }

std::thread::id GetCurrentThreadId() { return std::this_thread::get_id(); }

}  // namespace flare::fiber
