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

#ifndef FLARE_RPC_BUILTIN_DETAIL_PROF_UTILITY_H_
#define FLARE_RPC_BUILTIN_DETAIL_PROF_UTILITY_H_

#include <string>

namespace flare::rpc::builtin {

// Get path of this executable.
std::string ReadProcPath();

// Execute `command`.
bool PopenNoShellCompat(const std::string& command, std::string* result,
                        int* exit_code);

}  // namespace flare::rpc::builtin

#endif  // FLARE_RPC_BUILTIN_DETAIL_PROF_UTILITY_H_
