
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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_DETAIL_UTILITY_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_DETAIL_UTILITY_H_

#include <limits>

#include "flare/base/logging.h"

namespace flare::protobuf {

// Copy `src` to `dest`, with bounds check.
//
// This method is used when our own definition uses a wider type than the what
// the protocol use (e.g., `correlation_id`).
//
// TODO(luobogao): If makes sense, move it into `flare/base`.
template <class T, class U>
inline void SafeCopy(T src, U* dest) {
  FLARE_CHECK_LE(src, std::numeric_limits<U>::max(), "Overflow.");
  FLARE_CHECK_GE(src, std::numeric_limits<U>::min(), "Underflow.");
  *dest = src;
}

}  // namespace flare::protobuf

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_DETAIL_UTILITY_H_
