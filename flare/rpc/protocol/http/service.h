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

#ifndef FLARE_RPC_PROTOCOL_HTTP_SERVICE_H_
#define FLARE_RPC_PROTOCOL_HTTP_SERVICE_H_

#include <deque>
#include <memory>
#include <regex>  // TODO(luobogao): Try [hyperscan](https://github.com/intel/hyperscan).
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "flare/base/maybe_owning.h"
#include "flare/rpc/protocol/http/http_filter.h"
#include "flare/rpc/protocol/http/http_handler.h"
#include "flare/rpc/protocol/stream_service.h"

namespace flare::http {

// This class simply forward HTTP requests to the handlers registered by the
// user. The framework also registers handler for requests to several predefined
// paths. (e.g. `/inspect`)
//
// Indeed HTTP could run on UDP (QUIC), but for that matter, QUIC itself is
// treated as a stream-oriented transport, and should be implemented as a
// (subclass of) `StreamConnection` (which in turns, is turned into an
// interface, instead of being a concrete class.). Therefore it's still
// handleable by a `StreamService`.
class Service : public flare::StreamService {
 public:
  Service();

  // Filters are always called unconditionally. So make sure not to delay to
  // much in them.
  void AddFilter(MaybeOwning<HttpFilter> filter);  // Filter priority?

  // Precise match takes precedence. Regular expressions are only tested if
  // there's no exact match.
  void AddHandler(std::string path, MaybeOwning<HttpHandler> handler);
  void AddHandler(std::regex path_regex, MaybeOwning<HttpHandler> handler);
  void AddPrefixHandler(std::string path_prefix,
                        MaybeOwning<HttpHandler> handler);

  // For requests that are not otherwise handled by handlers registered above,
  // they're handed to this handler.
  void SetDefaultHandler(MaybeOwning<HttpHandler> handler);

  const experimental::Uuid& GetUuid() const noexcept override;

  bool Inspect(const Message& message, const Controller& controller,
               InspectionResult* result) override;

  bool ExtractCall(const std::string& serialized,
                   const std::vector<std::string>& serialized_pkt_ctxs,
                   ExtractedCall* extracted) override;

  // Forward messages to handlers and write responses to wire.
  //
  // A 404 HTTP message is sent out via `from` if no handler is able to handle
  // the message. (Therefore it's impossible to have multiple `http::Service`
  // running on the same port.)
  ProcessingStatus FastCall(
      std::unique_ptr<Message>* request,
      const FunctionView<std::size_t(const Message&)>& writer,
      Context* context) override;

  ProcessingStatus StreamCall(
      AsyncStreamReader<std::unique_ptr<Message>>* input_stream,
      AsyncStreamWriter<std::unique_ptr<Message>>* output_stream,
      Context* context) override;

  void Stop() override;
  void Join() override;

 private:
  HttpHandler* FindHandler(std::string_view uri);

  HttpFilter::Action RunFilters(HttpRequest* request, HttpResponse* response,
                                HttpServerContext* context);
  void RunHandler(const HttpRequest& request, HttpResponse* response,
                  HttpServerContext* context);

  void CompleteBinlogPostOperation(const HttpRequest& req,
                                   const HttpResponse& resp,
                                   const HttpServerContext& context);

 private:
  std::vector<MaybeOwning<HttpFilter>> filters_;

  // std::unordered_map does not support `is_transparent`, so we hack around
  // here.
  std::deque<std::string> exact_paths_storage_;
  std::unordered_map<std::string_view, MaybeOwning<HttpHandler>> exact_paths_;
  std::vector<std::pair<std::string, MaybeOwning<HttpHandler>>> prefix_paths_;
  std::vector<std::pair<std::regex, MaybeOwning<HttpHandler>>> regex_paths_;

  MaybeOwning<HttpHandler> default_handler_;
};

}  // namespace flare::http

#endif  // FLARE_RPC_PROTOCOL_HTTP_SERVICE_H_
