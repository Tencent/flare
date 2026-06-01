// Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_FIBER_DETAIL_FIBER_CONTEXT_H_
#define FLARE_FIBER_DETAIL_FIBER_CONTEXT_H_

extern "C" {

// Defined in `flare/fiber/detail/{arch}/*.S`.
//
// `returns_twice` is the same attribute setjmp/vfork carry: it tells the
// compiler that on return from this call, *all* register/memory state may
// have changed (we may even be running on a different OS thread now). Without
// it, on weakly-typed-TLS targets (aarch64 / ppc64le / TSan) the compiler is
// free to CSE thread_local reads across the context switch and observe the
// previous worker's `current_fiber`, which used to manifest as a flurry of
// `FLARE_CHECK_EQ(caller, GetCurrentFiberEntity())` failures and abort the
// whole fiber test suite.
[[gnu::returns_twice]] void jump_context(void** self, void* to, void* context);

// See https://man7.org/linux/man-pages/man3/makecontext.3.html
void* make_context(void* sp, std::size_t size, void (*start_proc)(void*));

} // extern "C"

#endif  // FLARE_FIBER_DETAIL_FIBER_CONTEXT_H_
