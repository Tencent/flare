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

#include "flare/base/internal/curl.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "curl/curl.h"

#include "flare/base/deferred.h"
#include "flare/base/logging.h"
#include "flare/base/never_destroyed.h"

using namespace std::literals;

namespace flare::internal {

namespace {

HttpPostMockHandler s_http_post_mock_handler;

HttpGetMockHandler s_http_get_mock_handler;

size_t HttpWriteCallback(char* ptr, size_t size, size_t nmemb, void* pstr) {
  auto bytes = size * nmemb;
  static_cast<std::string*>(pstr)->append(ptr, bytes);
  return bytes;
}

#define CURL_SHARE_SETOPT(handle, opt, val) \
  FLARE_CHECK_EQ(CURLSHE_OK, curl_share_setopt(handle, opt, val))

static NeverDestroyed<std::array<std::mutex, CURL_LOCK_DATA_LAST>>
    share_handle_mutex;

void AcquireLockOfShareHandle(CURL* handle, curl_lock_data data,
                              curl_lock_access access, void* userptr) {
  (*share_handle_mutex)[data].lock();
}

void ReleaseLockOfShareHandle(CURL* handle, curl_lock_data data,
                              void* userptr) {
  (*share_handle_mutex)[data].unlock();
}

CURLSH* GetCurlShareHandle() {
  using Handle = std::unique_ptr<CURLSH, decltype(&curl_share_cleanup)>;

  static NeverDestroyed<Handle> share([] {
    Handle share{curl_share_init(), &curl_share_cleanup};

    // DNS resolution is a pain, share it for better robustness.
    CURL_SHARE_SETOPT(share.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    // Sharing SSL session helps perf. a bit.
    CURL_SHARE_SETOPT(share.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    // @sa: https://curl.se/libcurl/c/curl_share_setopt.html
    //
    // > Note that due to a known bug, it is not safe to share connections this
    // > way between multiple concurrent threads.
    //
    // Therefore we don't share connections here.

    // Theoretically we can use a per-thread share handle for better
    // scalability, in trade of smaller share-scope. Here we don't care about
    // scalability much, yet we do want to reduce DNS resolution overhead (as it
    // can lead to timeout) as much as possible. Thus we use a global share
    // handle with lock here.
    CURL_SHARE_SETOPT(share.get(), CURLSHOPT_LOCKFUNC,
                      AcquireLockOfShareHandle);
    CURL_SHARE_SETOPT(share.get(), CURLSHOPT_UNLOCKFUNC,
                      ReleaseLockOfShareHandle);
    return share;
  }());
  return share->get();
}

#define CURL_SETOPT(handle, opt, val) \
  FLARE_CHECK_EQ(CURLE_OK, curl_easy_setopt(handle, opt, val))

}  // namespace

Expected<std::string, int> HttpPost(const std::string& uri,
                                    const std::vector<std::string>& headers,
                                    const std::string& body,
                                    std::chrono::nanoseconds timeout) {
  if (s_http_post_mock_handler) {
    return s_http_post_mock_handler(uri, headers, body, timeout);
  }

  std::string result;
  curl_slist* hdrs = curl_slist_append(nullptr, "Expect:");
  ScopedDeferred _{[&] { curl_slist_free_all(hdrs); }};
  for (auto&& s : headers) {
    hdrs = curl_slist_append(hdrs, s.c_str());
  }

  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl{curl_easy_init(),
                                                           &curl_easy_cleanup};
  CURL_SETOPT(curl.get(), CURLOPT_URL, uri.c_str());
  CURL_SETOPT(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
  CURL_SETOPT(curl.get(), CURLOPT_POSTFIELDSIZE_LARGE, body.size());
  CURL_SETOPT(curl.get(), CURLOPT_HTTPHEADER, hdrs);
  CURL_SETOPT(curl.get(), CURLOPT_WRITEFUNCTION, HttpWriteCallback);
  CURL_SETOPT(curl.get(), CURLOPT_WRITEDATA, &result);
  CURL_SETOPT(curl.get(), CURLOPT_NOSIGNAL, 1);
  CURL_SETOPT(curl.get(), CURLOPT_TIMEOUT_MS, timeout / 1ms);
  CURL_SETOPT(curl.get(), CURLOPT_SHARE, GetCurlShareHandle());
  CURL_SETOPT(curl.get(), CURLOPT_DNS_CACHE_TIMEOUT, 10min / 1s);

  auto rc = curl_easy_perform(curl.get());
  if (rc != CURLE_OK) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to call [{}]: [#{}] {}", uri, rc,
                                   curl_easy_strerror(rc));
    return -rc;
  }
  long resp_code;  // NOLINT: `CURLINFO_RESPONSE_CODE` needs a `long`..
  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &resp_code);
  if (resp_code != 200) {
    return resp_code;
  }
  return result;
}

Expected<std::string, int> HttpGet(const std::string& uri,
                                   std::chrono::nanoseconds timeout) {
  if (s_http_get_mock_handler) {
    return s_http_get_mock_handler(uri, timeout);
  }

  std::string result;
  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl{curl_easy_init(),
                                                           &curl_easy_cleanup};
  CURL_SETOPT(curl.get(), CURLOPT_URL, uri.c_str());
  CURL_SETOPT(curl.get(), CURLOPT_WRITEFUNCTION, HttpWriteCallback);
  CURL_SETOPT(curl.get(), CURLOPT_WRITEDATA, &result);
  CURL_SETOPT(curl.get(), CURLOPT_NOSIGNAL, 1);
  CURL_SETOPT(curl.get(), CURLOPT_TIMEOUT_MS, timeout / 1ms);
  auto rc = curl_easy_perform(curl.get());
  if (rc != CURLE_OK) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to call [{}]: [#{}] {}", uri, rc,
                                   curl_easy_strerror(rc));
    return -rc;
  }
  long resp_code;  // NOLINT: `CURLINFO_RESPONSE_CODE` needs a `long`..
  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &resp_code);
  if (resp_code != 200) {
    return resp_code;
  }
  return result;
}

void SetHttpPostMockHandler(HttpPostMockHandler&& http_post_mock_handler) {
  s_http_post_mock_handler = std::move(http_post_mock_handler);
}

void SetHttpGetMockHandler(HttpGetMockHandler&& http_get_mock_handler) {
  s_http_get_mock_handler = std::move(http_get_mock_handler);
}

}  // namespace flare::internal
