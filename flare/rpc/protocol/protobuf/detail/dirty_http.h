
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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_DETAIL_DIRTY_HTTP_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_DETAIL_DIRTY_HTTP_H_

#include <optional>
#include <string>
#include <string_view>

#include "flare/base/logging.h"
#include "flare/base/string.h"

namespace flare::protobuf {

// Some dirty-and-quick HTTP implementations.
//
// Implementations here are by no means a complete HTTP parserk, the sole
// purpose of them are to help us parsing PRC packets carried by HTTP mesages.

// This method helps us to read fields from an HTTP header in a "rough" way.
// It's by no means conformant, but it satisfy our need and fast enough.
//
// Note that this method does not handle overlapped field name correctly. For
// the moment we don't have to deal with that though.
//
// Returns empty string if not found.
std::string_view TryGetHeaderRoughly(const std::string& header,
                                     const std::string& key);

template <class T>
std::optional<T> TryGetHeaderRoughly(const std::string& header,
                                     const std::string& key) {
  return TryParse<T>(TryGetHeaderRoughly(header, key));
}

// @sa: `common/rpc/http_rpc_protocol.h`
//
// We need `std::string` here for perf. reasons.
static const std::string kRpcHttpHeaderSeqNo = "Rpc-SeqNo";
static const std::string kRpcHttpHeaderErrorCode = "Rpc-Error-Code";
static const std::string kRpcHttpHeaderErrorReason = "Rpc-Error-Reason";
static const std::string kRpcHttpHeaderRpcTimeout = "Rpc-Timeout";
static const std::string kRpcHttpHeaderRegressionUserData =
    "Rpc-Regression-UserData";
static const std::string kContentLength = "Content-Length";
static const std::string kContentType = "Content-Type";
static const std::string kTransferEncoding = "Transfer-Encoding";

}  // namespace flare::protobuf

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_DETAIL_DIRTY_HTTP_H_
