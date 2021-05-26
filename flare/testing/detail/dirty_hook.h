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

#ifndef FLARE_TESTING_DETAIL_DIRTY_HOOK_H_
#define FLARE_TESTING_DETAIL_DIRTY_HOOK_H_

namespace flare::testing::detail {

// Hook function located at `fptr`.
//
// Note that this method is BY NO MEANS a general hook implementation. To
// implement a "production-ready" hook library, at least the following issues
// should be addressed:
//
// - Be able to call the original implementation without restoring the hook (to
//   avoid race condition). Ususally this means the hook library should move the
//   opcodes overwritten to somewhere else *and* fix IP-relative addressing
//   (which requires a disassembler library).
//
// - Avoid overwrite opcodes when some (other) threads are executing them.
//
// - Do not mutate register. In case the user want to install a hook at the
//   middle of a function, no register is really "volatile".
//
// - Handle several corner cases such as when the function to be hooked is too
//   small to put our "jump" opcodes in.
//
// For our purpose (testing only), we don't take any of the issues above into
// consideration. Besides, we provide no way to call original implementation
// until the hook is restored.
//
// Returns a handle that can be used to restore the hook.
void* InstallHook(void* fptr, void* to);

// Restore a hook installed by `InstallHook`.
void UninstallHook(void* handle);

}  // namespace flare::testing::detail

#endif  // FLARE_TESTING_DETAIL_DIRTY_HOOK_H_
