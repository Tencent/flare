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

#ifndef FLARE_TESTING_COS_MOCK_H_
#define FLARE_TESTING_COS_MOCK_H_

#include <chrono>
#include <tuple>
#include <utility>

#include "googletest/gmock/gmock.h"

#include "flare/base/demangle.h"
#include "flare/base/function.h"
#include "flare/base/internal/lazy_init.h"
#include "flare/base/logging.h"
#include "flare/base/type_index.h"
#include "flare/net/cos/channel.h"
#include "flare/testing/detail/gmock_actions.h"
#include "flare/testing/detail/implicitly_casting.h"

// Usage: `FLARE_EXPECT_COS_OP(OperationName)...`
//
// Note that in order to intercept a COS operation, you need to extract
// `OperationName` from COS request type. e.g. `OperationName` for operation
// whose request type is `CosDeleteObjectRequest` would be `DeleteObject`.
//
// You can either return a fake result via `flare::testing::Return` or handle
// the request yourself via `flare::testing::HandleCosOp`.
//
// Currently the following are supported:
//
// - `flare::testing::Return(const flare::CosXxxResult& resp)`: Complete COS
//    response with the given response.
//
// - `flare::testing::Return(const flare::Status& errors)`: Fail the request
//    with error.
//
// - `flare::testing::HandleCosOp(F&& handler)`: Provides a handler for handling
//   COS request. The handler is expected to handle a signature as:
//
//   ```cpp
//   flare::Status Handler(const flare::CosXxxRequest&, flare::CosXxxResult*);
//   ```
//
//   or:
//
//   ```cpp
//   flare::Status Handler(const flare::CosXxxRequest&, flare::CosXxxResult*,
//                         const flare::cos::CosTask::Options& options);
//   ```

#define FLARE_EXPECT_COS_OP(OperationName)                                     \
  ::testing::Mock::AllowLeak(::flare::internal::LazyInit<                      \
                             ::flare::testing::detail::MockCosChannel>());     \
  EXPECT_CALL(                                                                 \
      *::flare::internal::LazyInit<                                            \
          ::flare::testing::detail::MockCosChannel>(),                         \
      Perform(::testing::_ /* self, ignored. */,                               \
              ::flare::testing::detail::RequestTypeMatcher(                    \
                  ::flare::GetTypeIndex<flare::Cos##OperationName##Request>(), \
                  "flare::Cos" #OperationName "Request") /* op */,             \
              ::testing::_ /* result, ignored */,                              \
              ::testing::_ /* options, ignored */,                             \
              ::testing::_ /* timeout, ignored */,                             \
              ::testing::_ /* done, ignored */))

// This helps you to handle COS operations yourself.
template <class F>
auto HandleCosOp(F&& handler);

////////////////////////////////////////
// Implementation goes below.         //
////////////////////////////////////////

namespace flare::testing {

namespace detail {

class MockCosChannel : public cos::Channel {
 public:
  MOCK_METHOD6(Perform, void(const cos::Channel*, const cos::CosOperation& op,
                             cos::CosOperationResult* result,
                             const cos::CosTask::Options& options,
                             std::chrono::nanoseconds timeout,
                             Function<void(Status)>* done));

  using GMockActionArguments =
      std::tuple<const cos::CosOperation&, cos::CosOperationResult*,
                 const cos::CosTask::Options&, std::chrono::nanoseconds,
                 Function<void(Status)>*>;

  template <class T, class = std::enable_if_t<
                         std::is_base_of_v<cos::CosOperationResult, T>>>
  static void GMockActionReturn(const GMockActionArguments& arguments,
                                const T& result) {
    *static_cast<T*>(std::get<1>(arguments)) = result;
    (*std::get<4>(arguments))(Status());
  }

  static void GMockActionReturn(const GMockActionArguments& arguments,
                                const Status& status);
};

MATCHER_P2(RequestTypeMatcher, type_disambiguator /* ignored */,
           expecting_req_type, "" /* desc */) {
  // Comparing type name. I suspect this (demangled name of type) is
  // implementation-defined behavior.
  return GetTypeName(arg) == expecting_req_type;
}

ACTION_P(HandleCosOpImpl, handler) {
  (*arg5)(
      handler(ImplicitlyCasting(&arg1), detail::ImplicitlyCasting(arg2), arg3));
}

template <class F>
constexpr auto invocable_without_opts_v =
    std::is_invocable_v<F, detail::ImplicitlyCasting<cos::CosOperation>,
                        detail::ImplicitlyCasting<cos::CosOperationResult>>;

template <class F>
auto HandleCosOpDispatch(
    F&& handler, std::enable_if_t<invocable_without_opts_v<F>>* = nullptr) {
  return HandleCosOpImpl(
      [h = std::forward<F>(handler)](auto&& x, auto&& y, auto&& opts) {
        return h(x, y);
      });
}

template <class F>
auto HandleCosOpDispatch(
    F&& handler, std::enable_if_t<!invocable_without_opts_v<F>>* = nullptr) {
  return HandleCosOpImpl(std::forward<F>(handler));
}

}  // namespace detail

template <class F>
auto HandleCosOp(F&& handler) {
  return detail::HandleCosOpDispatch(std::forward<F>(handler));
}

template <class T>
struct MockImplementationTraits;

template <>
struct MockImplementationTraits<cos::Channel> {
  using type = detail::MockCosChannel;
};

}  // namespace flare::testing

#endif  // FLARE_TESTING_COS_MOCK_H_
