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

#ifndef FLARE_NET_INTERNAL_HTTP_TASK_H_
#define FLARE_NET_INTERNAL_HTTP_TASK_H_

#include <chrono>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "curl/curl.h"

#include "flare/base/buffer.h"
#include "flare/base/expected.h"
#include "flare/base/function.h"
#include "flare/base/object_pool.h"
#include "flare/base/status.h"
#include "flare/net/http/types.h"

namespace flare::internal {

struct HttpTaskCallContext;

// Task for HttpEngine::StartTask.
class HttpTask {
 public:
  HttpTask();
  ~HttpTask();
  HttpTask(HttpTask&&) noexcept;

  // Must be called prior to setting `body`.
  void SetMethod(HttpMethod method);

  // You should at least set url and timeout.
  void SetUrl(const std::string& url);
  void SetTimeout(std::chrono::nanoseconds timeout);
  void SetBody(std::string body);
  void SetBody(NoncontiguousBuffer body);
  void AddHeader(const std::string& header);

  CURL* GetNativeHandle() noexcept;

 private:
  friend class HttpEngine;
  HttpMethod method_ = HttpMethod::Unspecified;
  PooledPtr<HttpTaskCallContext> ctx_;
  std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> hdrs_{
      nullptr, &curl_slist_free_all};
};

// Http task completion result.
class HttpTaskCompletion {
 public:
  explicit HttpTaskCompletion(HttpTaskCallContext* ctx);
  ~HttpTaskCompletion();
  HttpTaskCompletion(HttpTaskCompletion&&) noexcept;

  HttpStatus status() noexcept;
  HttpVersion version() noexcept;
  std::vector<std::string>* headers() noexcept;
  NoncontiguousBuffer* body() noexcept;

  CURL* GetNativeHandle() noexcept;

 private:
  PooledPtr<HttpTaskCallContext> ctx_;
  NoncontiguousBuffer body_;
};

// FOR INTERNAL USE ONLY.
struct HttpTaskCallContext {
  HttpTaskCallContext();
  ~HttpTaskCallContext();
  void Reset();

  CURL* curl_handler;
  std::unique_ptr<NoncontiguousBufferBuilder> body;
  std::vector<std::string> headers;
  Function<void(Expected<HttpTaskCompletion, Status>)> done;
  std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> hdrs;
  struct RequestBody {
    NoncontiguousBuffer buffer;
    NoncontiguousBuffer::const_iterator current_block;
    std::size_t buffer_block_inner_pos = 0;
  };
  RequestBody request_body;
};

}  // namespace flare::internal

namespace flare {

template <>
struct PoolTraits<internal::HttpTaskCallContext> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 8192;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 1024;
  static constexpr auto kTransferBatchSize = 2048;
  static void OnPut(internal::HttpTaskCallContext* cw) { cw->Reset(); }
};

}  // namespace flare

#endif  // FLARE_NET_INTERNAL_HTTP_TASK_H_
