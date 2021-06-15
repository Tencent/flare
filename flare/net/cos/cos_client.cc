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

#include "flare/net/cos/cos_client.h"

#include <string>
#include <string_view>

#include "gflags/gflags.h"

#include "flare/base/string.h"
#include "flare/net/cos/cos_channel.h"

using namespace std::literals;

DEFINE_int32(flare_cos_client_default_timeout_ms, 10000,
             "Default timeout for COS client.");

namespace flare {

namespace {

cos::Channel* mock_channel;

}  // namespace

bool CosClient::Open(const std::string& uri, const Options& options) {
  constexpr auto kDelimiter = "://"sv;
  auto uri_view = std::string_view(uri);
  auto delim_pos = uri_view.find(kDelimiter);
  FLARE_CHECK_NE(delim_pos, std::string::npos, "Invalid COS URI: [{}].",
                 uri_view);
  auto scheme = uri_view.substr(0, delim_pos);
  auto rest = uri_view.substr(delim_pos + kDelimiter.size());

  options_ = options;
  task_opts_.bucket = options_.bucket;
  task_opts_.secret_id = options_.secret_id;
  task_opts_.secret_key = options_.secret_key;

  if (scheme == "cos") {
    channel_ = std::make_unique<cos::CosChannel>();
    // The `rest` is region name.
    task_opts_.region = rest;
  } else if (scheme == "cos-polaris") {
    auto sep_pos = rest.find('/');
    FLARE_CHECK_NE(sep_pos, std::string_view::npos, "Invalid COS URI: [{}].",
                   uri_view);
    auto channel = std::make_unique<cos::CosChannel>();
    if (!channel->OpenPolaris(std::string(rest.substr(0, sep_pos)))) {
      return false;
    }
    channel_ = std::move(channel);
    // `rest` is 12345:56789/ap-guangzhou, we need to extract the region name.
    task_opts_.region = rest.substr(sep_pos + 1);
  } else if (scheme == "mock") {
    FLARE_CHECK(mock_channel,
                "COS mock channel is not registered. Have you forgotten to "
                "link against `//flare/testing:cos_mock`?");
    channel_ = MaybeOwning(non_owning, mock_channel);
    // `task_opts_.region` is not filled.
  } else {
    FLARE_CHECK(0, "Unexpected COS URI scheme [{}].", scheme);
  }
  return true;
}

void CosClient::RegisterMockChannel(cos::Channel* channel) {
  mock_channel = channel;
}

}  // namespace flare
