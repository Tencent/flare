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

#ifndef FLARE_RPC_PROTOCOL_HTTP_BUILTIN_PROF_CPU_HANDLER_H_
#define FLARE_RPC_PROTOCOL_HTTP_BUILTIN_PROF_CPU_HANDLER_H_

#include <map>
#include <mutex>
#include <string>

#include "gtest/gtest_prod.h"

#include "flare/rpc/http_handler.h"

namespace flare::rpc::builtin {

// Handler of `/prof/cpu`. Automatically registered by `Server`.
class ProfCpuHandler : public HttpHandler {
 public:
  explicit ProfCpuHandler(std::string uri_prefix);
  void OnGet(const HttpRequest& request, HttpResponse* response,
             HttpServerContext* context) override;

 private:
  FRIEND_TEST(ProfCpuHandler, All);

  void DoStart(const HttpRequest& request, HttpResponse* response,
               HttpServerContext* context);
  void DoView(const HttpRequest& request, HttpResponse* response,
              HttpServerContext* context);
  void DoStop(const HttpRequest& request, HttpResponse* response,
              HttpServerContext* context);

  enum class ResponseErrorCode {
    Succ,
    ErrorPath,
    ErrorAlreadyStart,
    ErrorProfileStart,
    ErrorNotStart,
    ErrorProf,
  };
  void SetBodyWithCode(std::string* body, ResponseErrorCode code);

  std::mutex profile_lock_;
  bool running = false;
  std::string proc_path_;

  std::string uri_prefix_;
  std::map<std::string, detail::FunctorHttpHandlerImpl::Impl>
      prof_path_handler_;
};

}  // namespace flare::rpc::builtin

#endif  // FLARE_RPC_PROTOCOL_HTTP_BUILTIN_PROF_CPU_HANDLER_H_
