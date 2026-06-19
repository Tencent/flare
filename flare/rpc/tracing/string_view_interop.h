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

#ifndef FLARE_RPC_TRACING_STRING_VIEW_INTEROP_H_
#define FLARE_RPC_TRACING_STRING_VIEW_INTEROP_H_

#include <string_view>

#include "opentelemetry/nostd/string_view.h"

// `opentelemetry`'s `nostd::string_view` only adopts `std::string_view` when the
// standard one isn't available; flare always builds with C++17, but `nostd`
// still doesn't offer a `std::string_view` conversion in either direction. These
// helpers bridge the two without copying.

namespace flare::tracing {

inline std::string_view ToStd(opentelemetry::nostd::string_view s) noexcept {
  return std::string_view(s.data(), s.size());
}

inline opentelemetry::nostd::string_view ToOtel(std::string_view s) noexcept {
  return opentelemetry::nostd::string_view(s.data(), s.size());
}

}  // namespace flare::tracing

#endif  // FLARE_RPC_TRACING_STRING_VIEW_INTEROP_H_
