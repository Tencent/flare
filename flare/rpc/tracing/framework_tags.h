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

#ifndef FLARE_RPC_TRACING_FRAMEWORK_TAGS_H_
#define FLARE_RPC_TRACING_FRAMEWORK_TAGS_H_

#include <string_view>

namespace flare::tracing::ext {

// Here we defined some tags recognized by Flare framework. `TracingOpsProvider`
// must translate the tags declared here to names recognized by its own
// implementation.
//
// They share the `flare.` prefix so `detail::IsFrameworkTag` can recognize them
// (@sa: `tracing_ops.cc`).

// Return value of (remote) method invocation.
inline constexpr std::string_view kInvocationStatus = "flare.invocation_status";

// If the trace is deemed special, a tracking id is tagged on it. (Some internal
// frameworks called it as "dyeing key", I'm not very satisfied with that name.)
inline constexpr std::string_view kTrackingId = "flare.tracking_id";

}  // namespace flare::tracing::ext

#endif  // FLARE_RPC_TRACING_FRAMEWORK_TAGS_H_
