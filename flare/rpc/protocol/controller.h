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

#ifndef FLARE_RPC_PROTOCOL_CONTROLLER_H_
#define FLARE_RPC_PROTOCOL_CONTROLLER_H_

#include <memory>
#include <string>
#include <utility>

#include "flare/base/casting.h"

namespace flare {

// The `Controller` controls a single RPC. It also serves as "context" for
// protocols to pass / retrieve information between its methods.
class Controller : public ExactMatchCastable {
 public:
  virtual ~Controller() = default;

  // For server-side protocol, tracing context is produced by the protocol, and
  // used by framework to decode the span for distributed tracing.
  //
  // For client-side protocol, the framework uses this method to pass span
  // context to the protocol for serialization.
  const std::string& GetTracingContext() const noexcept { return tracing_ctx_; }
  void SetTracingContext(std::string ctx) { tracing_ctx_ = std::move(ctx); }
  // Provided for perf. reasons. This does look ugly but...
  //
  // Frankly I just want to use a public member field here.
  std::string* MutableTracingContext() noexcept { return &tracing_ctx_; }

  // If set, the other side (normally the server we just called) are forcing us
  // to report the trace. This normally occurs when the backend failed to
  // satisfy our call for some reason.
  bool IsTraceForciblySampled() const noexcept { return forcibly_sampled_; }

  // Set whether the other side should report the trace unconditionally. The
  // caller is responsible for making sure it does not generate too many
  // reports.
  void SetTraceForciblySampled(bool f) noexcept { forcibly_sampled_ = f; }

 private:
  std::string tracing_ctx_;
  bool forcibly_sampled_{false};
};

// Factory for creating controllers.
class ControllerFactory {
 public:
  virtual ~ControllerFactory() = default;

  // Create a new controller.
  //
  // TODO(luobogao): Consider use object pool here.
  virtual std::unique_ptr<Controller> Create(bool streaming_call) const = 0;

  // A null factory that always returns `nullptr`.
  //
  // Note that unless the protocol you're implementing does not uses
  // "controller", you shouldn't use this null factory.
  static const ControllerFactory* null_factory;
};

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_CONTROLLER_H_
