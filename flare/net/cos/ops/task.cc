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

#include "flare/net/cos/ops/task.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "flare/base/buffer.h"
#include "flare/base/crypto/md5.h"
#include "flare/base/encoding/base64.h"
#include "flare/base/string.h"
#include "flare/net/cos/signature.h"

namespace flare::cos {

CosTask::CosTask(const Options* options) : options_(options) {}

const CosTask::Options& CosTask::options() const noexcept { return *options_; }

void CosTask::set_method(HttpMethod method) { method_ = method; }

void CosTask::set_uri(const std::string& uri) {
  auto domain_start = uri.find("://");
  FLARE_CHECK_NE(domain_start, std::string::npos, "Invalid URL [{}]", uri);
  domain_start += 3;  // `://`.
  auto domain_end = uri.find('/', domain_start);
  FLARE_CHECK_NE(domain_end, std::string::npos, "Invalid URL [{}]", uri);
  host_ = uri.substr(domain_start, domain_end - domain_start);
  uri_ = uri;
}

void CosTask::set_body(std::string body) {
  NoncontiguousBufferBuilder builder;
  builder.Append(MakeForeignBuffer(std::move(body)));
  set_body(builder.DestructiveGet());
}

void CosTask::set_body(NoncontiguousBuffer body) { body_ = std::move(body); }

void CosTask::OverrideAccessPoint(const Endpoint& ap) {
  auto pos = uri_.find("://");
  FLARE_CHECK_NE(pos, std::string::npos);
  pos = uri_.find('/', pos + 3);
  FLARE_CHECK_NE(pos, std::string::npos);
  uri_ = Format("http://{}{}", ap.ToString(), uri_.substr(pos));
}

void CosTask::AddHeader(const std::string& header) {
  headers_.emplace_back(header);
}

internal::HttpTask CosTask::BuildTask() {
  internal::HttpTask task;

  // Suppress headers added by libcurl automatically. They would mass things up
  // in QCloud's signature algorithm.
  task.AddHeader("Host:");
  task.AddHeader("Accept:");
  task.AddHeader("Content-Type:");

  // Well this usually does more harm than good when interacting with COS.
  task.AddHeader("Expect:");

  // Apply everything that have been applied on us.
  task.SetMethod(method_);
  task.SetUrl(uri_);
  task.AddHeader("Host: " + host_);
  task.AddHeader("Content-Length: " + std::to_string(body_.ByteSize()));
  if (!body_.Empty()) {
    task.AddHeader("Content-MD5: " + EncodeBase64(Md5(body_)));
    task.SetBody(std::move(body_));
  }
  for (auto&& e : headers_) {
    task.AddHeader(e);
  }

  // And sign it.
  task.AddHeader(
      Format("Authorization: {}",
             GenerateCosAuthString(options_->secret_id, options_->secret_key,
                                   method_, uri_, headers_)));
  return task;
}

CosTaskCompletion::CosTaskCompletion(internal::HttpTaskCompletion&& comp)
    : CosTaskCompletion(comp.status(), comp.version(),
                        std::move(*comp.headers()), std::move(*comp.body())) {}

CosTaskCompletion::CosTaskCompletion(HttpStatus status, HttpVersion version,
                                     std::vector<std::string> headers,
                                     NoncontiguousBuffer buffer)
    : status_(status), version_(version), body_(std::move(buffer)) {
  for (auto&& str : headers) {
    std::string_view e = str;
    auto pos = e.find_first_of(':');
    FLARE_CHECK_NE(pos, std::string_view::npos, "Invalid header [{}].", e);
    headers_.Append(std::string(Trim(e.substr(0, pos))),
                    std::string(Trim(e.substr(pos + 1))));
  }
}

}  // namespace flare::cos
