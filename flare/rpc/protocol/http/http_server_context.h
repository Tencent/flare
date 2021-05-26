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

#ifndef FLARE_RPC_PROTOCOL_HTTP_HTTP_SERVER_CONTEXT_H_
#define FLARE_RPC_PROTOCOL_HTTP_HTTP_SERVER_CONTEXT_H_

#include <map>
#include <string>

#include "flare/base/net/endpoint.h"

namespace flare {

// Describes basic facts about an incoming HTTP request.
//
// And control some behaviors of the request, such as binlog...
struct HttpServerContext {
  //////////////////////////////////
  // Fields below are read-only.  //
  //////////////////////////////////

  Endpoint remote_peer;

  // true if this request is sampled.
  bool is_sampling_binlog;

  // Some timestamps about this request.
  std::chrono::steady_clock::time_point received_at;
  std::chrono::steady_clock::time_point dispatched_at;
  std::chrono::steady_clock::time_point parsed_at;

  ///////////////////////////////////
  // Fields below are write-only.  //
  ///////////////////////////////////

  // Set true to prevent this sampled request.
  bool abort_binlog_capture = false;
  // custom tags for dumping.
  // Ignored when is_sampling_binlog is false.
  std::map<std::string, std::string> binlog_tags;
};

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_HTTP_HTTP_SERVER_CONTEXT_H_
