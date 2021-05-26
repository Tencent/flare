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

#ifndef FLARE_NET_HTTP_HTTP_MESSAGE_H_
#define FLARE_NET_HTTP_HTTP_MESSAGE_H_

#include <optional>
#include <string>
#include <utility>

#include "flare/base/buffer.h"
#include "flare/net/http/http_headers.h"
#include "flare/net/http/types.h"

namespace flare::http {

// Logics shared by both HttpRequest & HttpResponse.
class HttpMessage {
 public:
  HttpVersion version() const noexcept { return version_; }
  void set_version(HttpVersion version) noexcept { version_ = version; }

  HttpHeaders* headers() noexcept { return &headers_; }
  const HttpHeaders* headers() const noexcept { return &headers_; }

  // If internally the body is stored as `NoncontiguousBuffer`, this method
  // flattens it.
  //
  // After calling this method, `noncontiguous_body()` is no longer available.
  std::string* body() noexcept {
    return FLARE_LIKELY(body_str_) ? &*body_str_ : StringifyBody();
  }
  // CAUTION: NOT THREAD-SAFE.
  const std::string* body() const noexcept {
    return const_cast<HttpMessage*>(this)->body();
  }
  void set_body(NoncontiguousBuffer&& nb) noexcept {
    body_str_ = std::nullopt;
    body_ = std::move(nb);
  }
  // Invalidates `noncontiguous_body()`.
  void set_body(std::string s) {
    body_ = std::nullopt;
    body_str_ = std::move(s);
  }

  // Get the body size without stringify body.
  // You can not call set_body at the same time.
  std::size_t body_size() const noexcept {
    if (FLARE_LIKELY(body_str_)) {
      return body_str_->size();
    } else if (FLARE_LIKELY(body_)) {
      return body_->ByteSize();
    } else {
      return 0;
    }
  }

  // Not always present, it's provided only for performance sensitive & bulk
  // transfer. If the body is deemed large enough to be stored non-contiguous,
  // it's store here. In such cases, `body()` internally flatten the buffer (and
  // cache the result) and return the result. When possible, dealing with the
  // buffer returned here can boost performance.
  //
  // ALWAYS TEST FOR `nullptr` BEFORE USING THE RETURN VALUE.
  NoncontiguousBuffer* noncontiguous_body() noexcept {
    return body_ ? &*body_ : nullptr;
  }
  const NoncontiguousBuffer* noncontiguous_body() const noexcept {
    return const_cast<HttpMessage*>(this)->noncontiguous_body();
  }

  void clear() noexcept {
    headers_.clear();
    body_str_ = std::nullopt;
    body_ = std::nullopt;
  }

 private:
  std::string* StringifyBody();

 private:
  HttpVersion version_{HttpVersion::Unspecified};
  HttpHeaders headers_;
  mutable std::optional<std::string> body_str_;
  std::optional<NoncontiguousBuffer> body_;
};

}  // namespace flare::http

#endif  // FLARE_NET_HTTP_HTTP_MESSAGE_H_
