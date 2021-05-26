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

#include "flare/rpc/protocol/protobuf/call_context.h"

namespace flare::protobuf {

MaybeOwning<google::protobuf::Message>
ProactiveCallContext::GetOrCreateResponse() {
  if (expecting_stream) {
    FLARE_CHECK(response_prototype);
    return MaybeOwning(owning, response_prototype->New());
  } else {
    auto rc = std::exchange(response_ptr, nullptr);
    FLARE_CHECK(rc);  // Otherwise it's a bug in `StreamCallGate`.
    return MaybeOwning(non_owning, rc);
  }
}

}  // namespace flare::protobuf
