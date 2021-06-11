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

#include "opentracing-cpp/span.h"  // FIXME: `opentracing/string_view.h`

// It's unfortunate that `opentracing-cpp` comes with its own `string_view`, and
// does not interoperate with `std::`'s well, so we provide some helpers here.

namespace flare::tracing {

inline bool operator==(const std::string_view& s,
                       const opentracing::string_view& o) {
  return s == std::string_view(o.data(), o.size());
}

inline bool operator==(const opentracing::string_view& o,
                       const std::string_view& s) {
  return s == std::string_view(o.data(), o.size());
}

}  // namespace flare::tracing

#endif  // FLARE_RPC_TRACING_STRING_VIEW_INTEROP_H_
