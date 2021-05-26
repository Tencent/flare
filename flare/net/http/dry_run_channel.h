// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_NET_HTTP_DRY_RUN_CHANNEL_H_
#define FLARE_NET_HTTP_DRY_RUN_CHANNEL_H_

#include <string>

#include "flare/net/http/http_client.h"

namespace flare::http {

// This channel is only used when performing dry-run.
//
// FOR INTERNAL USE ONLY.
class DryRunChannel : public detail::HttpChannel {
 public:
  void AsyncGet(const std::string& url, const HttpClient::Options& opts,
                const HttpClient::RequestOptions& request_options,
                HttpClient::ResponseInfo* response_info,
                Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&&
                    done) override;

  void AsyncPost(const std::string& url, const HttpClient::Options& opts,
                 std::string data,
                 const HttpClient::RequestOptions& request_options,
                 HttpClient::ResponseInfo* response_info,
                 Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&&
                     done) override;

  void AsyncRequest(
      const std::string& protocol, const std::string& host,
      const HttpClient::Options& opts, const HttpRequest& request,
      const HttpClient::RequestOptions& request_options,
      HttpClient::ResponseInfo* response_info,
      Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&& done)
      override;
};

}  // namespace flare::http

#endif  // FLARE_NET_HTTP_DRY_RUN_CHANNEL_H_
