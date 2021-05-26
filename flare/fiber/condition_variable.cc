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

#include "flare/fiber/condition_variable.h"

namespace flare::fiber {

void ConditionVariable::notify_one() noexcept { impl_.notify_one(); }

void ConditionVariable::notify_all() noexcept { impl_.notify_all(); }

void ConditionVariable::wait(std::unique_lock<Mutex>& lock) {
  impl_.wait(lock);
}

}  // namespace flare::fiber
