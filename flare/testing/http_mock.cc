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

#include "flare/testing/http_mock.h"

#include "flare/base/internal/lazy_init.h"
#include "flare/init/on_init.h"

namespace flare::testing::detail {

FLARE_ON_INIT(0 /* doesn't matter */, [] {
  flare::detail::RegisterMockHttpChannel(internal::LazyInit<HttpMockChannel>());
});

HttpMockChannel::HttpMockChannel() {
  // This can't be done at global initialization time due to static global
  // variable initialization order fiasco.
  ::testing::Mock::AllowLeak(this);
}

void HttpMockChannel::GMockActionReturn(const GMockActionArguments& arguments,
                                        HttpResponse resp) {
  Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>* done =
      std::get<5>(arguments);
  (*done)(resp);
}

void HttpMockChannel::GMockActionReturn(const GMockActionArguments& arguments,
                                        HttpClient::ErrorCode err) {
  Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>* done =
      std::get<5>(arguments);
  (*done)(err);
}

void HttpMockChannel::GMockActionReturn(const GMockActionArguments& arguments,
                                        HttpResponse resp,
                                        HttpClient::ResponseInfo info) {
  *std::get<4>(arguments) = info;
  Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>* done =
      std::get<5>(arguments);
  (*done)(resp);
}

}  // namespace flare::testing::detail
