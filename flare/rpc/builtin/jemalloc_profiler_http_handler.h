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

#ifndef FLARE_RPC_BUILTIN_JEMALLOC_PROFILER_HTTP_HANDLER_H_
#define FLARE_RPC_BUILTIN_JEMALLOC_PROFILER_HTTP_HANDLER_H_

#include <map>
#include <mutex>
#include <string>

#include "flare/rpc/http_handler.h"

//////////////////////////////////////////////////
// YOU DON'T HAVE TO INCLUDE THIS HEADER.       //
// LINKING IT INTO YOUR PROGRAM IS SUFFICIENT.  //
//////////////////////////////////////////////////

namespace flare::rpc::builtin {

// Handler of `/prof/mem`.
// Linked with jemalloc.
class JemallocProfilerHttpHandler : public HttpHandler {
 public:
  explicit JemallocProfilerHttpHandler(std::string uri_prefix);
  void OnGet(const HttpRequest& request, HttpResponse* response,
             HttpServerContext* context) override;

 private:
  void DoStart(const HttpRequest& request, HttpResponse* response,
               HttpServerContext* context);
  void DoView(const HttpRequest& request, HttpResponse* response,
              HttpServerContext* context);
  void DoStop(const HttpRequest& request, HttpResponse* response,
              HttpServerContext* context);

  enum class ResponseErrorCode {
    Succ,
    ErrorPath,
    ErrorDisabled,
    ErrorAlreadyStart,
    ErrorMallctl,
    ErrorWriteFile,
    ErrorNotStart,
    ErrorProf,
  };
  void SetBodyWithCode(std::string* body, ResponseErrorCode code);

  bool enabled_;
  std::mutex profile_lock_;
  std::string proc_path_;
  bool running_ = false;

  std::string uri_prefix_;
  std::map<std::string, detail::FunctorHttpHandlerImpl::Impl>
      prof_path_handler_;
};

}  // namespace flare::rpc::builtin

#endif  // FLARE_RPC_BUILTIN_JEMALLOC_PROFILER_HTTP_HANDLER_H_
