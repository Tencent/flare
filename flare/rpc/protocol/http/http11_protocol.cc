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

// TODO(luobogao): Remove everything here about `chunked` encoding, they're not
// supported by upper layer API. We supported it for the sake of `http+pb`
// protocol (which no longer depends on us).

#include "flare/rpc/protocol/http/http11_protocol.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "flare/base/deferred.h"
#include "flare/base/string.h"
#include "flare/rpc/protocol/http/buffer_io.h"
#include "flare/rpc/protocol/http/message.h"

using namespace std::literals;

namespace flare::http {

FLARE_RPC_REGISTER_SERVER_SIDE_STREAM_PROTOCOL_ARG("http", Http11Protocol,
                                                   true);
FLARE_RPC_REGISTER_CLIENT_SIDE_STREAM_PROTOCOL_ARG("http", Http11Protocol,
                                                   false);

namespace {

struct OnWireMessage : public Message {
  OnWireMessage() { SetRuntimeTypeTo<OnWireMessage>(); }
  std::uint64_t GetCorrelationId() const noexcept override {
    return kNonmultiplexableCorrelationId;
  }
  Type GetType() const noexcept override { return Type::Single; }

  HeaderBlock header_block;
  NoncontiguousBuffer body;
};

// HACK here. We'll use a more generic way to exclude several HTTP request
// being handled by others.
const std::string_view kClobberedStartLines[] = {"POST /rpc/"sv,
                                                 "POST /__rpc_service__"sv};

bool IsMessageClobbered(const std::string_view& view) {
  for (auto&& e : kClobberedStartLines) {
    if (StartsWith(view, e)) {
      return true;
    }
  }
  return false;
}

bool ExpectingContentLength(const std::string_view& view) {
  if (StartsWith(view, "GET"sv) || StartsWith(view, "HEAD"sv)) {
    return false;
  }

  static constexpr auto kHttpResponsePrefix = "HTTP/1.1 "sv;
  if (StartsWith(view, kHttpResponsePrefix)) {
    std::string_view status_sv = view.substr(kHttpResponsePrefix.size(), 3);
    auto status_opt = TryParse<int>(status_sv);
    if (status_opt && (*status_opt == underlying_value(HttpStatus::NoContent) ||
                       (*status_opt >= 100 && *status_opt < 200))) {
      return false;
    }  // What if the status is invalid (non-numeric, e.g.)?
  }
  return true;
}

}  // namespace

const StreamProtocol::Characteristics& Http11Protocol::GetCharacteristics()
    const {
  static const Characteristics cs = {.name = "HTTP/1.1"};
  return cs;
}

const MessageFactory* Http11Protocol::GetMessageFactory() const {
  return MessageFactory::null_factory;
}

const ControllerFactory* Http11Protocol::GetControllerFactory() const {
  return ControllerFactory::null_factory;
}

StreamProtocol::MessageCutStatus Http11Protocol::TryCutMessage(
    NoncontiguousBuffer* buffer, std::unique_ptr<Message>* message) {
  if (!parsed_header_block_.first) {
    HeaderBlock header_block;
    // Let's copy the header out first.
    auto status = ReadHeader(*buffer, &header_block);
    if (FLARE_UNLIKELY(status == ReadStatus::UnexpectedFormat)) {
      return MessageCutStatus::ProtocolMismatch;
    } else if (FLARE_UNLIKELY(status == ReadStatus::Error)) {
      return MessageCutStatus::Error;  // NotIdentified?
    }
    if (status == ReadStatus::NoEnoughData) {
      return MessageCutStatus::NotIdentified;
    }
    FLARE_CHECK(status == ReadStatus::OK);

    // HACK here.
    if (IsMessageClobbered({header_block.first.get(), header_block.second})) {
      return MessageCutStatus::ProtocolMismatch;
    }

    // We don't want to parse it again.
    parsed_header_block_ = std::move(header_block);
  }

  auto header_view = std::string_view(parsed_header_block_.first.get(),
                                      parsed_header_block_.second);

  // Let's see if an entire message is received.
  auto body_size = flare::TryParse<std::size_t>(
      ReadFieldFromRawBytes(header_view, "Content-Length"));
  if (!body_size) {
    if (!ExpectingContentLength(header_view)) {
      body_size = 0;
    } else {
      if (header_view.find("chunked") != std::string_view::npos) {
        FLARE_LOG_ERROR_ONCE(
            "It seems a message with `chunked` encoding is received. We do not "
            "support `chunked` encoding (yet).");
      }
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Messages without \"Content-Length\" header are not supported.");
      return MessageCutStatus::Error;
    }
  }
  if (buffer->ByteSize() < header_view.size() + *body_size) {
    return MessageCutStatus::NeedMore;
  }

  // Cut if off then.
  buffer->Skip(parsed_header_block_.second);

  auto msg = std::make_unique<OnWireMessage>();
  // Moved away, we're fresh now.
  msg->header_block = std::move(parsed_header_block_);
  msg->body = buffer->Cut(*body_size);
  *message = std::move(msg);

  return MessageCutStatus::Cut;
}

bool Http11Protocol::TryParse(std::unique_ptr<Message>* message,
                              Controller* controller) {
  auto msg = cast<OnWireMessage>(**message);
  std::string_view start_line;

  if (server_side_) {
    auto parsed = std::make_unique<HttpRequestMessage>();
    if (!ParseMessagePartial(std::move(msg->header_block), &start_line,
                             parsed->headers())) {
      FLARE_LOG_WARNING_EVERY_SECOND("Invalid HTTP header received.");
      return false;
    }

    HttpVersion version;
    HttpMethod method;
    std::string_view uri;
    if (!ParseRequestStartLine(start_line, &version, &method, &uri)) {
      FLARE_LOG_WARNING_EVERY_SECOND("Invalid HTTP header received.");
      return false;
    }
    parsed->request()->set_version(version);
    parsed->request()->set_method(method);
    parsed->request()->set_uri(std::string(uri));
    parsed->request()->set_body(std::move(msg->body));

    *message = std::move(parsed);
    return true;
  } else {
    auto parsed = std::make_unique<HttpResponseMessage>();
    if (!ParseMessagePartial(std::move(msg->header_block), &start_line,
                             parsed->headers())) {
      FLARE_LOG_WARNING_EVERY_SECOND("Invalid HTTP header received.");
      return false;
    }
    HttpStatus status;
    if (!ParseResponseStartLine(start_line, &status)) {
      FLARE_LOG_WARNING_EVERY_SECOND("Invalid HTTP header received.");
      return false;
    }
    parsed->response()->set_status(status);
    parsed->response()->set_body(std::move(msg->body));

    *message = std::move(parsed);
    return true;
  }

  FLARE_UNREACHABLE();
}

void Http11Protocol::WriteMessage(const Message& message,
                                  NoncontiguousBuffer* buffer,
                                  Controller* controller) {
  NoncontiguousBufferBuilder builder;

  if (auto p = dyn_cast<HttpRequestMessage>(message)) {
    http::WriteMessage(*p->request(), &builder);
  } else if (auto p = dyn_cast<HttpResponseMessage>(message)) {
    http::WriteMessage(*p->response(), &builder);
  } else {
    FLARE_CHECK(0, "Unexpected message type [{}].", GetTypeName(message));
  }
  *buffer = builder.DestructiveGet();
}

}  // namespace flare::http
