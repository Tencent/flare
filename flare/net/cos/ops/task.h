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

#ifndef FLARE_NET_COS_OPS_TASK_H_
#define FLARE_NET_COS_OPS_TASK_H_

#include <string>
#include <vector>

#include "flare/base/net/endpoint.h"
#include "flare/net/http/http_headers.h"
#include "flare/net/internal/http_task.h"

namespace flare::cos {

// Extends `internal::HttpTask`, to do necessary bookkeeping during constructing
// HTTP request.
class CosTask {
 public:
  struct Options {
    // Credential.
    std::string secret_id, secret_key;

    // e.g., `ap-guangzhou`.
    std::string region;

    // Empty if no default was set.
    //
    // Unless the user does not set bucket explicit on this operation, the
    // implementation should ignore this field.
    std::string bucket;
  };

  // Note that `options` is kept by reference, it's the caller's responsibility
  // to make sure `options` outlives this object.
  explicit CosTask(const Options* options);

  // Options assigned with this task.
  //
  // This is provided by the framework and available to `CosOperation`s to use.
  const Options& options() const noexcept;

  // Modifiers.
  void set_method(HttpMethod method);
  void set_uri(const std::string& uri);
  void AddHeader(const std::string& header);
  void set_body(std::string body);
  void set_body(NoncontiguousBuffer body);

  // Mostly used in internal network. This allows us to use non-public COS
  // access point (if possible) for better performance.
  //
  // Due to implementation limitations, this method may only be called after
  // `set_uri` is called.
  void OverrideAccessPoint(const Endpoint& ap);

  // Accessors, they're mostly used by UTs.
  HttpMethod method() const noexcept { return method_; }
  const std::string& uri() const noexcept { return uri_; }
  const std::vector<std::string>& headers() const noexcept { return headers_; }
  const NoncontiguousBuffer& body() const noexcept { return body_; }

  // Build an HTTP task to send to COS server.
  //
  // For Flare's internal use.
  internal::HttpTask BuildTask();

 private:
  const Options* options_;
  HttpMethod method_;
  std::string uri_;
  std::string host_;
  std::vector<std::string> headers_;
  NoncontiguousBuffer body_;
};

// To make things symmetric, we use `CosTaskCompletion` for HTTP response.
class CosTaskCompletion {
 public:
  explicit CosTaskCompletion(internal::HttpTaskCompletion&& comp);

  // Accessors.
  HttpStatus status() const noexcept { return status_; }
  HttpVersion version() const noexcept { return version_; }
  HttpHeaders* headers() noexcept { return &headers_; }
  NoncontiguousBuffer* body() noexcept { return &body_; }

  // This overload is for testing purpose only. It's used by UT to artificially
  // create "HTTP response".
  CosTaskCompletion(HttpStatus status, HttpVersion version,
                    std::vector<std::string> headers,
                    NoncontiguousBuffer buffer);

 private:
  HttpStatus status_;
  HttpVersion version_;
  HttpHeaders headers_;
  NoncontiguousBuffer body_;
};

}  // namespace flare::cos

#endif  // FLARE_NET_COS_OPS_TASK_H_
