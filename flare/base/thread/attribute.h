// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_BASE_THREAD_ATTRIBUTE_H_
#define FLARE_BASE_THREAD_ATTRIBUTE_H_

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace flare {

// Some helper methods for manipulating threads.

// Set affinity of the calling thread.
//
// Returns error number, or 0 on success.
int TrySetCurrentThreadAffinity(const std::vector<int>& affinity);

// Set affinity of the calling thread.
//
// Abort on failure.
void SetCurrentThreadAffinity(const std::vector<int>& affinity);

// Get affinity of the calling thread.
std::vector<int> GetCurrentThreadAffinity();

// Set name of the calling thread.
//
// Error, if any, is ignored.
void SetCurrentThreadName(const std::string& name);

}  // namespace flare

#endif  // FLARE_BASE_THREAD_ATTRIBUTE_H_
