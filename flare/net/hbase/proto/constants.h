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

#ifndef FLARE_NET_HBASE_PROTO_CONSTANTS_H_
#define FLARE_NET_HBASE_PROTO_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

// Constants defined here are NOT intended for public use.

namespace flare::hbase::constants {

const std::size_t kRpcHeaderLength = 4;

// Comes from `HConstants`.
inline const char kRpcHeader[] = {'H', 'B', 'a', 's'};  // RPC_HEADER
constexpr std::uint8_t kRpcVersion = 0;                 // CURRENT_VERSION

// Comes from `AuthMethod`.
constexpr std::uint8_t kAuthMethodSimple = 80;    // SIMPLE
constexpr std::uint8_t kAuthMethodKerberos = 81;  // KERBEROS
constexpr std::uint8_t kAuthMethodDigest = 82;    // DIGEST

// Exception class names in `org.apache.hadoop.hbase.ipc`.
inline const char* kBadAuthException =
    "org.apache.hadoop.hbase.ipc.BadAuthException";
// CAUTION: Our old codebase used `RpcException` here.
inline const char* kCallTimeoutException =
    "org.apache.hadoop.hbase.ipc.CallTimeoutException";
inline const char* kStoppedRpcClientException =
    "org.apache.hadoop.hbase.ipc.StoppedRpcClientException";
inline const char* kServerNotRunningYetException =
    "org.apache.hadoop.hbase.ipc.ServerNotRunningYetException";
inline const char* kFallbackDisallowedException =
    "org.apache.hadoop.hbase.ipc.FallbackDisallowedException";
inline const char* kWrongVersionException =
    "org.apache.hadoop.hbase.ipc.WrongVersionException";
inline const char* kUnsupportedCompressionCodecException =
    "org.apache.hadoop.hbase.ipc.UnsupportedCompressionCodecException";
inline const char* kRemoteWithExtrasException =
    "org.apache.hadoop.hbase.ipc.RemoteWithExtrasException";
inline const char* kFatalConnectionException =
    "org.apache.hadoop.hbase.ipc.FatalConnectionException";
inline const char* kFailedServerException =
    "org.apache.hadoop.hbase.ipc.FailedServerException";
inline const char* kUnsupportedCellCodecException =
    "org.apache.hadoop.hbase.ipc.UnsupportedCellCodecException";
inline const char* kCellScannerButNoCodecException =
    "org.apache.hadoop.hbase.ipc.CellScannerButNoCodecException";
inline const char* kServerTooBusyException =
    "org.apache.hadoop.hbase.ipc.ServerTooBusyException";
inline const char* kCallCancelledException =
    "org.apache.hadoop.hbase.ipc.CallCancelledException";
inline const char* kUnsupportedCryptoException =
    "org.apache.hadoop.hbase.ipc.UnsupportedCryptoException";
inline const char* kCallerDisconnectedException =
    "org.apache.hadoop.hbase.ipc.CallerDisconnectedException";
inline const char* kTestRpcHandlerException =
    "org.apache.hadoop.hbase.ipc.TestRpcHandlerException";
inline const char* kUnknownServiceException =
    "org.apache.hadoop.hbase.ipc.UnknownServiceException";
inline const char* kEmptyServiceNameException =
    "org.apache.hadoop.hbase.ipc.EmptyServiceNameException";

}  // namespace flare::hbase::constants

#endif  // FLARE_NET_HBASE_PROTO_CONSTANTS_H_
