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

#ifndef FLARE_RPC_BINLOG_TAGS_H_
#define FLARE_RPC_BINLOG_TAGS_H_

#include <string>

namespace flare::binlog {

// To make serialization / deserialization easier, and for the sake of forward
// compatibility (adding new fields in `XxxCall` without affecting old binlog
// provider), we use "system tags" to store fields we're interested in
// `XxxCall`s.
//
// Note that these tags are stored separately from user tags to avoid name
// collision.

namespace tags {

// These names are for exposition only, and may change without further notice.
inline static const std::string kOperationName = "operation_name";

// I'd like to use integer here but not all protocols agree on this. (Notably
// HBase, who uses "exception class name" instead.)
inline static const std::string kInvocationStatus = "invocation_status";

///////////////////////////////////////////////
// Fields below are for outgoing call only.  //
///////////////////////////////////////////////

// Name of service where the binlog was captured.
inline static const std::string kServiceName = "service_name";

// HTTP URI / RPC URI / MySQL conn string / ...
inline static const std::string kUri = "uri";

///////////////////////////////////////////////
// Fields below are for incoming call only.  //
///////////////////////////////////////////////

// UUID of `StreamService` which handled the captured incoming call.
inline static const std::string kHandlerUuid = "handler_uuid";

// Address of local peer (listening address) and remote peer.
inline static const std::string kLocalPeer = "local_peer";
inline static const std::string kRemotePeer = "remote_peer";

}  // namespace tags

}  // namespace flare::binlog

#endif  // FLARE_RPC_BINLOG_TAGS_H_
