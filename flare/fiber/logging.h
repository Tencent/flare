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

#ifndef FLARE_FIBER_LOGGING_H_
#define FLARE_FIBER_LOGGING_H_

#include <string>
#include <string_view>

#include "flare/base/string.h"

namespace flare::fiber {

// Add a logging prefix to current fiber.
//
// Usage: `AddLoggingItemToExecution(some_id_var);`
void AddLoggingItemToFiber(std::string s);

// Add a logging prefix to current "execution context".
void AddLoggingItemToExecution(const std::string& s);

// Same as above, except that what's actually get added is in form of `key:
// value`.
template <class T>
void AddLoggingTagToFiber(std::string_view key, const T& value) {
  return AddLoggingItemToFiber(Format("{}: {}", key, value));
}

template <class T>
void AddLoggingTagToExecution(std::string_view key, const T& value) {
  return AddLoggingItemToExecution(Format("{}: {}", key, value));
}

}  // namespace flare::fiber

#endif  // FLARE_FIBER_LOGGING_H_
