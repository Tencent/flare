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

#include "flare/rpc/binlog/packet_desc.h"

#include <string>
#include <variant>

#include "google/protobuf/message.h"
#include "google/protobuf/util/json_util.h"

#include "flare/base/buffer.h"
#include "flare/base/buffer/zero_copy_stream.h"

namespace flare::binlog {

experimental::LazyEval<std::string> ProtoPacketDesc::Describe() const {
  if (message.index() == 0) {
    std::string result;
    auto status = google::protobuf::util::MessageToJsonString(
        *std::get<0>(message), &result);
    FLARE_CHECK(status.ok(), "Failed to serialize message: {}",
                status.error_message());
    return result;
  } else {
    FLARE_CHECK_EQ(message.index(), 1);
    // TODO(luobogao): We still can implement this, as we can infer message type
    // from `message.instance`.
    return R"|({"(error)":"Cannot JSON-ify raw message bytes."})|";
  }
}

NoncontiguousBuffer ProtoPacketDesc::WriteMessage() const {
  if (message.index() == 0) {
    NoncontiguousBufferBuilder builder;
    NoncontiguousBufferOutputStream nbos(&builder);
    FLARE_CHECK(std::get<0>(message));
    FLARE_CHECK(std::get<0>(message)->SerializeToZeroCopyStream(&nbos));
    nbos.Flush();
    return builder.DestructiveGet();
  } else {
    FLARE_CHECK_EQ(message.index(), 1);
    return *std::get<1>(message);  // Never null.
  }
}

}  // namespace flare::binlog
