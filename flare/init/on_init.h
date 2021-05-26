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

#ifndef FLARE_INIT_ON_INIT_H_
#define FLARE_INIT_ON_INIT_H_

#include <cstdint>
#include <utility>

#include "flare/base/function.h"
#include "flare/base/internal/macro.h"

// Usage:
//
// - FLARE_ON_INIT(init, fini = nullptr): This overload defaults `priority` to
//   0.
// - FLARE_ON_INIT(priority, init, fini = nullptr)
//
// This macro registers a callback that is called in `flare::Start` (after
// `main` is entered`.). The user may also provide a finalizer, which is called
// before leaving `flare::Start`, in opposite order.
//
// `priority` specifies relative order between callbacks. Callbacks with smaller
// `priority` are called earlier. Order between callbacks with same priority is
// unspecified and may not be relied on.
//
// It explicitly allowed to use this macro *without* carring a dependency to
// `//flare:init`.
//
// For UT writers: If, for any reason, you cannot use `FLARE_TEST_MAIN` as your
// `main` in UT, you need to call `RunAllInitializers()` / `RunAllFinalizers()`
// yourself when entering / leaving `main`.
#define FLARE_ON_INIT(...)                                                 \
  static ::flare::detail::OnInitRegistration FLARE_INTERNAL_PP_CAT(        \
      flare_on_init_registration_object_, __COUNTER__)(__FILE__, __LINE__, \
                                                       __VA_ARGS__);

namespace flare::internal {

// Registers a callback that's called before leaving `flare::Start`.
//
// These callbacks are called after all finalizers registered via
// `FLARE_ON_INIT`.
void SetAtExitCallback(Function<void()> callback);

}  // namespace flare::internal

// Implementation goes below.
namespace flare::detail {

// Called by `OnInitRegistration`.
void RegisterOnInitCallback(std::int32_t priority, Function<void()> init,
                            Function<void()> fini);

// Called by `flare::Start`.
void RunAllInitializers();
void RunAllFinalizers();

// Helper class for registering initialization callbacks at start-up time.
class OnInitRegistration {
 public:
  OnInitRegistration(const char* file, std::uint32_t line,
                     Function<void()> init, Function<void()> fini = nullptr)
      : OnInitRegistration(file, line, 0, std::move(init), std::move(fini)) {}

  OnInitRegistration(const char* file, std::uint32_t line,
                     std::int32_t priority, Function<void()> init,
                     Function<void()> fini = nullptr) {
    RegisterOnInitCallback(priority, std::move(init), std::move(fini));
  }
};

}  // namespace flare::detail

#endif  // FLARE_INIT_ON_INIT_H_
