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

#include "thirdparty/gflags/gflags.h"

#include "flare/base/enum.h"
#include "flare/base/thread/latch.h"
#include "flare/net/http/http_client.h"
#include "flare/init.h"

using namespace std::literals;

DEFINE_string(url, "", "Http request url.");
DEFINE_int32(timeout, 1000, "Timeout in ms.");

namespace example::http_echo {

int Entry(int argc, char** argv) {
  flare::HttpClient client;

  flare::HttpResponse response;
  flare::HttpClient::RequestOptions opts{
      .timeout = FLAGS_timeout * 1ms,
  };
  auto&& resp = client.Get(FLAGS_url, opts);
  if (!resp) {
    FLARE_LOG_INFO("Error code {}",
                   flare::HttpClient::ErrorCodeToString(resp.error()));
  } else {
    FLARE_LOG_INFO("Status code {}", underlying_value(resp->status()));
    FLARE_LOG_INFO("Response body {}", *resp->body());
  }

  return 0;
}

}  // namespace example::http_echo

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::http_echo::Entry);
}
