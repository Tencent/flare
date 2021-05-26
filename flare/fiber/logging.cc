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

#include "flare/fiber/logging.h"

#include <mutex>
#include <string>

#include "flare/base/internal/logging.h"
#include "flare/fiber/execution_context.h"
#include "flare/fiber/fiber_local.h"

namespace flare::fiber {

namespace {

class InterlockedLoggingPrefix {
 public:
  void Append(const std::string& s) {
    std::scoped_lock _(lock_);
    str_ += str_.empty() ? "[" : " [";
    str_ += s;
    str_ += "]";
  }

  std::string Get() const {
    std::scoped_lock _(lock_);
    return str_;
  }

 private:
  mutable std::mutex lock_;
  std::string str_;
};

// `FiberLocal<T>` is inherently thread-safe, no locking required.
FiberLocal<std::string> fiber_logging_prefix;

// For execution local, we need to grab a lock..
ExecutionLocal<InterlockedLoggingPrefix> execution_logging_prefix;

bool IsFiberPresent() {
  return fiber::detail::GetCurrentFiberEntity() != nullptr;
}

bool IsExecutionContextPresent() {
  return IsFiberPresent() && ExecutionContext::Current() != nullptr;
}

}  // namespace

void AddLoggingItemToFiber(std::string s) {
  auto&& prefix = *fiber_logging_prefix;
  prefix += prefix.empty() ? "[" : " [";
  prefix += s;
  prefix += "]";
}

void AddLoggingItemToExecution(const std::string& s) {
  execution_logging_prefix->Append(s);
}

}  // namespace flare::fiber

FLARE_INTERNAL_LOGGING_REGISTER_PREFIX_PROVIDER(0, [](std::string* str) {
  if (flare::fiber::IsFiberPresent()) {
    str->append(*flare::fiber::fiber_logging_prefix);
  }
})

FLARE_INTERNAL_LOGGING_REGISTER_PREFIX_PROVIDER(1, [](std::string* str) {
  if (flare::fiber::IsExecutionContextPresent()) {
    // We're in a execution context then.
    str->append(flare::fiber::execution_logging_prefix->Get());
  }
})
