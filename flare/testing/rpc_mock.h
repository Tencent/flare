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

#ifndef FLARE_TESTING_RPC_MOCK_H_
#define FLARE_TESTING_RPC_MOCK_H_

#include <string>
#include <tuple>
#include <utility>

#include "thirdparty/googletest/gmock/gmock.h"
#include "thirdparty/protobuf/service.h"
#include "thirdparty/protobuf/util/message_differencer.h"

#include "flare/base/callback.h"
#include "flare/base/down_cast.h"
#include "flare/base/internal/lazy_init.h"
#include "flare/base/status.h"
#include "flare/base/string.h"
#include "flare/rpc/protocol/protobuf/mock_channel.h"
#include "flare/rpc/protocol/protobuf/rpc_meta.pb.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/rpc_server_controller.h"
#include "flare/testing/detail/gmock_actions.h"  // Included for the user.
#include "flare/testing/detail/implicitly_casting.h"

// Usage: `FLARE_EXPECT_RPC(XxxService::Method, {matcher})...`
//
// To manually provide a response (either a successful one or an error), use
// `FLARE_EXPECT_RPC(...).WillXxx(flare::testing::Return(...))`.
//
// Currently the following are supported:
//
// - `flare::testing::Return(const google::protobuf::Message&)`: Complete the
//   RPC with the given message.
//
// - `flare::testing::Return(rpc::Status)`: Fail the rpc with the given status.
//
// - `flare::testing::Return(const std::string& desc)`: Fail the RPC with
//   `rpc::STATUS_FAILED`. `desc` is filled as `ErrorText()`.
//
// - `flare::testing::Return(rpc::Status const std::string& desc)`: Fail the RPC
//   with `status`. `desc` is filled as `ErrorText()`.
//
// - `flare::testing::Return(const flare::Status& status)`: Fail the RPC with
//   status value and error text in `status`.
//
// To manually handle an RPC, use:
//
// ```cpp
// FLARE_EXPECT_RPC(...).WillXxx(flare::testing::HandleRpc([] (
//     const XxxReq& req, XxxResp* resp, RpcServerController* server_ctlr) {
//   // ...
// });
// ```
#define FLARE_EXPECT_RPC(Method, RequestMatcher)                         \
  EXPECT_CALL(*::flare::internal::LazyInit<                              \
                  ::flare::testing::detail::MockRpcChannel>(),           \
              CallMethod(::testing::_,                                   \
                         ::flare::testing::detail::ServiceMethodNameEq(  \
                             &Method, #Method) /* method */,             \
                         ::testing::NotNull() /* controller, ignored */, \
                         RequestMatcher /* request */,                   \
                         ::testing::_ /* response, ignored */,           \
                         ::testing::_ /* done, ignored */))

namespace flare::testing {

// TODO(luobogao): Let's see how will descriptor be used in the output.
MATCHER_P(ProtoEq, expecting, " has content ") {
  return google::protobuf::util::MessageDifferencer::Equals(expecting, *arg);
}

// If you'd like to handle RPC yourself, you can use this action to instantiate
// an object that will be called for handling mocked RPC.
//
// You SHOULDN'T use `::testing::Invoke` to handle RPC. We do some bookkeeping
// internally, and `testing::Invoke` won't to that automagically for you.
//
// Due to technical limitations, `handler` is called with a proxy type instead
// of `XxxRequest` / `XxxResponse`. However, as long as you declare parameter
// type of `handler` explicitly, we'll handle the type conversion for you.
template <class F>
auto HandleRpc(F&& handler);

}  // namespace flare::testing

/////////////////////////////////
// Implementation goes below.  //
/////////////////////////////////

namespace flare::testing {

namespace detail {

// Adopted from gdt's rpc mock.

class MockRpcChannel : public protobuf::detail::MockChannel {
 public:
  MockRpcChannel();

  MOCK_METHOD6(CallMethod,
               void(const protobuf::detail::MockChannel*,
                    const google::protobuf::MethodDescriptor*,
                    google::protobuf::RpcController*,
                    const google::protobuf::Message*,
                    google::protobuf::Message*, google::protobuf::Closure*));

  using GMockActionArguments =
      std::tuple<const google::protobuf::MethodDescriptor*,
                 google::protobuf::RpcController*,
                 const google::protobuf::Message*, google::protobuf::Message*,
                 google::protobuf::Closure*>;

  static void GMockActionReturn(const GMockActionArguments& arguments,
                                const google::protobuf::Message& value);
  static void GMockActionReturn(const GMockActionArguments& arguments,
                                rpc::Status status);
  static void GMockActionReturn(const GMockActionArguments& arguments,
                                const std::string& desc);
  static void GMockActionReturn(const GMockActionArguments& arguments,
                                rpc::Status status, const std::string& desc);
  static void GMockActionReturn(const GMockActionArguments& arguments,
                                Status status);

  static void RunCompletionWith(RpcClientController* ctlr,
                                const rpc::RpcMeta& meta,
                                google::protobuf::Closure* done);

  static void CopyAttachment(const RpcClientController& from,
                             RpcServerController* to);
  static void CopyAttachment(const RpcServerController& from,
                             RpcClientController* to);
};

MATCHER_P2(ServiceMethodNameEq, type_disambiguator /* ignored */,
           expecting_method_cstr, "" /* desc */) {
  auto calling_method = arg->service()->name() + "::" + arg->name();
  std::string_view expecting_method = expecting_method_cstr;
  if (EndsWith(expecting_method, calling_method)) {
    auto pos = expecting_method.size() - calling_method.size();
    return pos == 0 ||  // Not further qualified.
           (pos >= 2 &&
            expecting_method.substr(pos - 2, 2) == "::");  // Qualified.
  }
  return false;
}

ACTION_P(HandleRpcImpl, handler) {
  // Copy input.
  auto client_ctlr = flare::down_cast<RpcClientController>(arg2);
  RpcServerController server_ctlr;
  MockRpcChannel::CopyAttachment(*client_ctlr, &server_ctlr);

  // Call user's callback.
  handler(ImplicitlyCasting(arg3), ImplicitlyCasting(arg4), &server_ctlr);

  // Copy output and completes this RPC.
  MockRpcChannel::CopyAttachment(server_ctlr, client_ctlr);
  rpc::RpcMeta meta;
  meta.mutable_response_meta()->set_status(server_ctlr.ErrorCode());
  meta.mutable_response_meta()->set_description(server_ctlr.ErrorText());
  detail::MockRpcChannel::RunCompletionWith(client_ctlr, meta, arg5);
}

}  // namespace detail

///////////////////////////////////////////////////////////
// DEPRECATED: ACTIONS BELOW ARE DEPRECATED IN FAVOR OF  //
// `flare::testing::Return(...)`.                        //
///////////////////////////////////////////////////////////

ACTION_P(Respond, value) {
  arg4->CopyFrom(value);
  rpc::RpcMeta meta;
  meta.mutable_response_meta()->set_status(rpc::STATUS_SUCCESS);
  detail::MockRpcChannel::RunCompletionWith(
      flare::down_cast<RpcClientController>(arg2), meta, arg5);
}

ACTION_P(FailWith, value) {
  rpc::RpcMeta meta;
  auto&& resp_meta = *meta.mutable_response_meta();
  if constexpr (std::is_same_v<std::decay_t<decltype(value)>, rpc::Status>) {
    resp_meta.set_status(value);
  } else if constexpr (std::is_convertible_v<std::decay_t<decltype(value)>,
                                             int>) {
    // Special logic for handling `gdt::ErrorCode`.
    resp_meta.set_status(static_cast<rpc::Status>(value));
  } else {
    resp_meta.set_status(rpc::STATUS_FAILED);
    resp_meta.set_description(value);
  }
  detail::MockRpcChannel::RunCompletionWith(
      flare::down_cast<RpcClientController>(arg2), meta, arg5);
}

ACTION_P2(FailWith, status, desc) {
  rpc::RpcMeta meta;
  auto&& resp_meta = *meta.mutable_response_meta();
  resp_meta.set_status(status);
  resp_meta.set_description(desc);
  detail::MockRpcChannel::RunCompletionWith(
      flare::down_cast<RpcClientController>(arg2), meta, arg5);
}

template <class F>
auto HandleRpc(F&& handler) {
  return detail::HandleRpcImpl(std::forward<F>(handler));
}

template <class T>
struct MockImplementationTraits;

template <>
struct MockImplementationTraits<protobuf::detail::MockChannel> {
  using type = detail::MockRpcChannel;
};

}  // namespace flare::testing

#endif  // FLARE_TESTING_RPC_MOCK_H_
