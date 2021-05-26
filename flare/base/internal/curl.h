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

#ifndef FLARE_BASE_INTERNAL_CURL_H_
#define FLARE_BASE_INTERNAL_CURL_H_

#include <chrono>
#include <string>
#include <vector>

#include "flare/base/expected.h"
#include "flare/base/function.h"

// Helper methods to make HTTP request via libcurl.

namespace flare::internal {

using HttpPostMockHandler = Function<Expected<std::string, int>(
    const std::string&, const std::vector<std::string>&, const std::string&,
    std::chrono::nanoseconds)>;

using HttpGetMockHandler = Function<Expected<std::string, int>(
    const std::string&, std::chrono::nanoseconds)>;

Expected<std::string, int> HttpPost(const std::string& uri,
                                    const std::vector<std::string>& headers,
                                    const std::string& body,
                                    std::chrono::nanoseconds timeout);

Expected<std::string, int> HttpGet(const std::string& uri,
                                   std::chrono::nanoseconds timeout);

void SetHttpPostMockHandler(HttpPostMockHandler&& http_post_mock_handler);

void SetHttpGetMockHandler(HttpGetMockHandler&& http_get_mock_handler);

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_CURL_H_
