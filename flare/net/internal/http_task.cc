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

#include "flare/net/internal/http_task.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

#include <algorithm>
#include <vector>

#include "flare/base/enum.h"
#include "flare/base/internal/lazy_init.h"

using namespace std::literals;

namespace flare::internal {

HttpTaskCallContext::HttpTaskCallContext()
    : curl_handler(curl_easy_init()),
      done(nullptr),
      hdrs(nullptr, &curl_slist_free_all) {}

HttpTaskCallContext::~HttpTaskCallContext() { curl_easy_cleanup(curl_handler); }

void HttpTaskCallContext::Reset() {
  curl_easy_reset(curl_handler);
  body = nullptr;
  headers.clear();
  done = nullptr;
  hdrs = nullptr;
  request_body.buffer.Clear();
}

HttpTask::HttpTask() : ctx_(object_pool::Get<HttpTaskCallContext>()) {}

HttpTask::~HttpTask() = default;

HttpTask::HttpTask(HttpTask&&) noexcept = default;

void HttpTask::SetUrl(const std::string& url) {
  FLARE_CHECK_EQ(
      CURLE_OK, curl_easy_setopt(ctx_->curl_handler, CURLOPT_URL, url.c_str()));
}

void HttpTask::SetMethod(HttpMethod method) {
  method_ = method;
  if (method == HttpMethod::Head) {
    FLARE_CHECK_EQ(CURLE_OK,
                   curl_easy_setopt(ctx_->curl_handler, CURLOPT_NOBODY, 1L));
  } else if (method == HttpMethod::Get) {
    FLARE_CHECK_EQ(CURLE_OK,
                   curl_easy_setopt(ctx_->curl_handler, CURLOPT_HTTPGET, 1L));
  } else if (method == HttpMethod::Post) {
    FLARE_CHECK_EQ(CURLE_OK,
                   curl_easy_setopt(ctx_->curl_handler, CURLOPT_POST, 1L));
  } else if (method == HttpMethod::Put) {
    FLARE_CHECK_EQ(CURLE_OK,
                   curl_easy_setopt(ctx_->curl_handler, CURLOPT_UPLOAD, 1L));
  } else if (method == HttpMethod::Delete) {
    FLARE_CHECK_EQ(CURLE_OK, curl_easy_setopt(ctx_->curl_handler,
                                              CURLOPT_CUSTOMREQUEST, "DELETE"));
  } else {
    FLARE_UNEXPECTED("Unsupported HTTP method #{}.", underlying_value(method));
  }
}

void HttpTask::SetTimeout(std::chrono::nanoseconds timeout) {
  FLARE_CHECK_EQ(CURLE_OK, curl_easy_setopt(ctx_->curl_handler,
                                            CURLOPT_TIMEOUT_MS, timeout / 1ms));
}

CURL* HttpTask::GetNativeHandle() noexcept { return ctx_->curl_handler; }

void HttpTask::SetBody(std::string body) {
  NoncontiguousBufferBuilder builder;
  builder.Append(MakeForeignBuffer(std::move(body)));
  SetBody(builder.DestructiveGet());
}

size_t HttpReadCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto body = reinterpret_cast<HttpTaskCallContext::RequestBody*>(userdata);
  if (body->current_block == body->buffer.end()) {
    return 0;
  }

  auto n_copy = std::min(
      body->current_block->size() - body->buffer_block_inner_pos, size * nmemb);
  memcpy(ptr, body->current_block->data() + body->buffer_block_inner_pos,
         n_copy);
  body->buffer_block_inner_pos += n_copy;
  if (body->buffer_block_inner_pos == body->current_block->size()) {
    body->buffer_block_inner_pos = 0;
    ++body->current_block;
  }
  return n_copy;
}

static int HttpSeekCallback(void* userdata, curl_off_t offset, int origin) {
  if (origin != SEEK_SET || offset < 0) {
    FLARE_NOT_IMPLEMENTED(
        "libcurl currently only passes SEEK_SET, and we only achieved this.");
  }
  auto body = reinterpret_cast<HttpTaskCallContext::RequestBody*>(userdata);
  body->current_block = body->buffer.begin();
  body->buffer_block_inner_pos = 0;
  while (offset > 0) {
    if (body->current_block == body->buffer.end()) {
      // Reach end, but still has offset, should this happen?
      return CURL_SEEKFUNC_OK;
    }
    if (offset < body->current_block->size()) {
      body->buffer_block_inner_pos = offset;
      return CURL_SEEKFUNC_OK;
    }
    offset -= body->current_block->size();
    ++body->current_block;
  }
  return CURL_SEEKFUNC_OK;
}

void HttpTask::SetBody(NoncontiguousBuffer body) {
  FLARE_CHECK(method_ != HttpMethod::Unspecified);
  FLARE_CHECK(method_ != HttpMethod::Head && method_ != HttpMethod::Get,
              "HEAD/GET request should not carry a body.");

  // Move the buffer in first.
  ctx_->request_body.buffer = std::move(body);

  if (method_ == HttpMethod::Post) {
    FLARE_CHECK_EQ(
        CURLE_OK,
        curl_easy_setopt(ctx_->curl_handler, CURLOPT_POSTFIELDSIZE_LARGE,
                         ctx_->request_body.buffer.ByteSize()));

    // POST-specific optimization.
    if (ctx_->request_body.buffer.ByteSize() ==
        ctx_->request_body.buffer.FirstContiguous().size()) {
      // It seems that CURLOPT_POSTFIELDS is faster than CURLOPT_READFUNCTION
      FLARE_CHECK_EQ(
          CURLE_OK,
          curl_easy_setopt(ctx_->curl_handler, CURLOPT_POSTFIELDS,
                           ctx_->request_body.buffer.FirstContiguous().data()));
      return;
    }  // Fall-though to "generic" way otherwise.
  } else if (method_ == HttpMethod::Put) {
    FLARE_CHECK_EQ(
        CURLE_OK, curl_easy_setopt(ctx_->curl_handler, CURLOPT_INFILESIZE_LARGE,
                                   ctx_->request_body.buffer.ByteSize()));
  } else {
    FLARE_UNEXPECTED("Unexpected HTTP method #{}.", underlying_value(method_));
  }

  ctx_->request_body.current_block = ctx_->request_body.buffer.begin();
  ctx_->request_body.buffer_block_inner_pos = 0;
  FLARE_CHECK_EQ(CURLE_OK,
                 curl_easy_setopt(ctx_->curl_handler, CURLOPT_READFUNCTION,
                                  HttpReadCallback));
  FLARE_CHECK_EQ(CURLE_OK,
                 curl_easy_setopt(ctx_->curl_handler, CURLOPT_READDATA,
                                  &ctx_->request_body));
  FLARE_CHECK_EQ(CURLE_OK,
                 curl_easy_setopt(ctx_->curl_handler, CURLOPT_SEEKFUNCTION,
                                  HttpSeekCallback));
  FLARE_CHECK_EQ(CURLE_OK,
                 curl_easy_setopt(ctx_->curl_handler, CURLOPT_SEEKDATA,
                                  &ctx_->request_body));
}

void HttpTask::AddHeader(const std::string& header) {
  hdrs_.reset(curl_slist_append(hdrs_.release(), header.c_str()));
}

HttpTaskCompletion::HttpTaskCompletion(HttpTaskCallContext* ctx)
    : ctx_(ctx), body_(ctx_->body->DestructiveGet()) {}

HttpTaskCompletion::~HttpTaskCompletion() = default;

HttpTaskCompletion::HttpTaskCompletion(HttpTaskCompletion&&) noexcept = default;

NoncontiguousBuffer* HttpTaskCompletion::body() noexcept { return &body_; }

std::vector<std::string>* HttpTaskCompletion::headers() noexcept {
  return &ctx_->headers;
}

HttpStatus HttpTaskCompletion::status() noexcept {
  // `CURLINFO_RESPONSE_CODE` needs a `long`..
  long resp_code;  // NOLINT
  curl_easy_getinfo(ctx_->curl_handler, CURLINFO_RESPONSE_CODE, &resp_code);
  return static_cast<HttpStatus>(resp_code);
}

CURL* HttpTaskCompletion::GetNativeHandle() noexcept {
  return ctx_->curl_handler;
}

HttpVersion TranslateCurlHttpVersion(
    long version /* libcurl uses `long` */) {  // NOLINT
  if (version == CURL_HTTP_VERSION_1_0) {
    return HttpVersion::V_1_0;
  } else if (version == CURL_HTTP_VERSION_1_1) {
    return HttpVersion::V_1_1;
  } else if (version == CURL_HTTP_VERSION_2_0) {
    return HttpVersion::V_2;
  } else if (version == CURL_HTTP_VERSION_3) {
    return HttpVersion::V_3;
  }
  FLARE_UNEXPECTED("Unrecognized HTTP version [{}] from libcurl.", version);
}

HttpVersion HttpTaskCompletion::version() noexcept {
  // `CURLOPT_HTTP_VERSION ` requires this.
  long version;  // NOLINT
  curl_easy_getinfo(ctx_->curl_handler, CURLINFO_HTTP_VERSION, &version);
  return TranslateCurlHttpVersion(version);
}

}  // namespace flare::internal
