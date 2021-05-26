// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_RPC_PROTOCOL_HTTP_HTTP_FILTER_H_
#define FLARE_RPC_PROTOCOL_HTTP_HTTP_FILTER_H_

#ifndef FLARE_HTTP_FILTER_SUPPRESS_INCLUDE_WARNING
#warning Use `flare/rpc/http_filter.h` instead.
#endif

#include "flare/net/http/http_request.h"
#include "flare/net/http/http_response.h"
#include "flare/rpc/protocol/http/http_server_context.h"

namespace flare {

// This class allows you to "filter" HTTP request before it's processed by
// corresponding handler.
//
// For the moment we don't support mutating the response after the request has
// been handled. That's a bit hard to do as we either have to:
//
// - Use a call-chain to pass response up (through the filter chain): This is a
//   bit hard to implement (as it will introduce a deep call chain), can hurt
//   performance, and can easily overwhelm runtime stack if there's enough
//   filters.
//
// - Or, pass contexts between pre-handler and post-handler: This requires
//   dynamic allocation in critical path.
//
// For the moment we don't see the need of mutating responses. Were that demand
// appears, we can change the name of this class to `HttpFastFilter` and
// introduce `HttpChainedFilter` for accomplishing it. (As for ordering, chained
// filter are always called after fast filters.).
class HttpFilter {
 public:
  virtual ~HttpFilter() = default;

  // Action to be taken by the framework.
  enum class Action {
    // Call next filter, or there is none, the actual HTTP handler.
    KeepProcessing,

    // Drop this request, nothing will be returned in this case. No futher
    // action (e.g. calling remaining filters, calling the actual HTTP handler)
    // is required.
    Drop,

    // Return immediately with what's filled in `response`, any pending filter
    // will not be called, neither will be the actual HTTP handler.
    EarlyReturn
  };

  // The framework calls this method before handing request to corresponding
  // handler.
  //
  // The implementation may mutate any of arguments if it deems fit. But be
  // caution not to confuse other filters / handler.
  virtual Action OnFilter(HttpRequest* request, HttpResponse* response,
                          HttpServerContext* context) = 0;
};

// TODO(luobogao): AsyncHttpFilter? I'm not sure if it's needed for now. It can
// be rather complicated.

// TODO(luobogao): Builtin filters, such as OA authentication.

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_HTTP_HTTP_FILTER_H_
