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

#ifndef FLARE_NET_HTTP_PACKET_DESC_H_
#define FLARE_NET_HTTP_PACKET_DESC_H_

#include <string>
#include <variant>

#include "flare/net/http/http_request.h"
#include "flare/net/http/http_response.h"
#include "flare/rpc/binlog/packet_desc.h"

namespace flare::http {

// Describes an HTTP packet.
struct PacketDesc : flare::binlog::TypedPacketDesc<PacketDesc> {
  std::variant<const HttpRequest*, const HttpResponse*> message;

  explicit PacketDesc(const HttpRequest& request) : message(&request) {}
  explicit PacketDesc(const HttpResponse& response) : message(&response) {}

  experimental::LazyEval<std::string> Describe() const override;

  // Serialize the message in HTTP/1.1 style.
  std::string ToString() const;
};

}  // namespace flare::http

#endif  // FLARE_NET_HTTP_PACKET_DESC_H_
