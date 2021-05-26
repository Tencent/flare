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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CONTROLLER_COMMON_INL_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CONTROLLER_COMMON_INL_H_

#include "flare/base/down_cast.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/rpc_controller_common.h"

namespace flare::protobuf {

template <class T>
TypedOutputStreamProvider<T>::TypedOutputStreamProvider(
    RpcControllerCommon* ctlr)
    : ctlr_(ctlr) {}

template <class T>
TypedOutputStreamProvider<T>::~TypedOutputStreamProvider() {
  FLARE_CHECK(last_sent_ || !ctlr_->use_eos_marker_,
              "Have you closed the `StreamWriter`?");
}

template <class T>
void TypedOutputStreamProvider<T>::SetExpiration(
    std::chrono::steady_clock::time_point expires_at) {
  ctlr_->output_stream_->SetExpiration(expires_at);
}

template <class T>
void TypedOutputStreamProvider<T>::Write(T object, bool last,
                                         Function<void(bool)> cb) {
  FLARE_CHECK(!last_sent_);
  auto msg = TranslateMessage(std::move(object), last);

  // Update the markers.
  first_sent_ = true;
  last_sent_ = last;

  // Unfortunately interface of `StreamWriter` and its provider does not align
  // well.
  if (last) {
    ctlr_->output_stream_->WriteLast(std::move(msg))
        .Then([cb = std::move(cb)](bool f) { cb(f); });
  } else {
    ctlr_->output_stream_->Write(std::move(msg))
        .Then([cb = std::move(cb)](bool f) { cb(f); });
  }
}

template <class T>
void TypedOutputStreamProvider<T>::Close(Function<void()> cb) {
  FLARE_CHECK(!last_sent_);  // The caller is asking for trouble otherwise.
  if (ctlr_->use_eos_marker_) {
    WriteEosMarker(std::move(cb));
  } else {
    if (!first_sent_ && ctlr_->ErrorCode()) {
      // In case we never sent anything out and the call is marked as a failed
      // one, we should sent a erroneous reply to caller.
      WriteEosMarker(std::move(cb));
    } else {
      // Otherwise we have no EOS marker to write, so just close the stream.
      ctlr_->output_stream_->Close().Then([cb = std::move(cb)] { cb(); });
    }
  }
}

template <class T>
std::unique_ptr<Message> TypedOutputStreamProvider<T>::TranslateMessage(
    T object, bool eos) {
  auto meta = object_pool::Get<rpc::RpcMeta>();
  *meta = ctlr_->meta_prototype_;
  auto msg = std::make_unique<protobuf::ProtoMessage>(
      std::move(meta), std::make_unique<T>(std::move(object)));

  if (!first_sent_) {
    // In case it's the first message in the stream, we set start-of-stream
    // marker.
    msg->meta->set_flags(rpc::MESSAGE_FLAGS_START_OF_STREAM);
  } else {
    FLARE_CHECK(!(msg->meta->flags() & rpc::MESSAGE_FLAGS_START_OF_STREAM));
  }

  if (eos) {
    // Same for end-of-stream marker.
    msg->meta->set_flags(msg->meta->flags() | rpc::MESSAGE_FLAGS_END_OF_STREAM);
  }
  return msg;
}

template <class T>
void TypedOutputStreamProvider<T>::WriteEosMarker(Function<void()> cb) {
  FLARE_CHECK(!last_sent_);
  auto meta = object_pool::Get<rpc::RpcMeta>();

  *meta = ctlr_->meta_prototype_;  // Correlation ID / status / ... are copied
                                   // from the prototype.
  meta->set_flags(rpc::MESSAGE_FLAGS_NO_PAYLOAD);
  meta->set_flags(meta->flags() | rpc::MESSAGE_FLAGS_END_OF_STREAM);
  if (!first_sent_) {
    // This should be rare, if we take this branch, the user close the stream
    // without sending out anything.
    meta->set_flags(meta->flags() | rpc::MESSAGE_FLAGS_START_OF_STREAM);
  }
  if (ctlr_->server_side_) {
    FLARE_CHECK(!meta->has_request_meta());
    auto&& resp_meta = *meta->mutable_response_meta();
    resp_meta.set_status(ctlr_->ErrorCode());
    if (ctlr_->Failed()) {
      resp_meta.set_description(ctlr_->ErrorText());
    }
  }

  last_sent_ = true;
  ctlr_->output_stream_
      ->WriteLast(
          std::make_unique<protobuf::ProtoMessage>(std::move(meta), nullptr))
      .Then([cb = std::move(cb)](bool) { cb(); });
}

template <class T>
TypedInputStreamProvider<T>::TypedInputStreamProvider(RpcControllerCommon* ctlr)
    : ctlr_(ctlr) {}

template <class T>
TypedInputStreamProvider<T>::~TypedInputStreamProvider() {
  FLARE_CHECK(closed_, "Have you closed the `StreamReader`?");
}

template <class T>
void TypedInputStreamProvider<T>::SetExpiration(
    std::chrono::steady_clock::time_point expires_at) {
  ctlr_->input_stream_->SetExpiration(expires_at);
}

template <class T>
void TypedInputStreamProvider<T>::Peek(
    Function<void(Expected<T, StreamError>*)> cb) {
  FLARE_CHECK(!"Peek() is not supported for end-user's use.");
}

template <class T>
void TypedInputStreamProvider<T>::Read(
    Function<void(Expected<T, StreamError>)> cb) {
  if (seen_inline_eos_) {
    closed_ = true;  // Implicitly closed.

    // The last message we read carries an end-of-stream marker, so we
    // synthesize one here.
    cb(StreamError::EndOfStream);
    return;
  }

  auto continuation = [this, cb = std::move(cb)](auto&& e) mutable {
    NotifyControllerProgress(e);

    // We only care about the case when `e`:
    //
    // - Itself is NOT an error;
    // - It does not carry a payload;
    // - But it indicates end-of-stream.
    //
    // In this case, what we know is different from what underlying stream
    // (i.e. `ctlr_->input_stream_`) knows, in that it would think the stream
    // is still valid, while our user would think the stream has been closed
    // (as he / she would see an end-of-stream error by the next time he / she
    // calls `Read()`), and won't call `Close()` himself / herself.
    //
    // If `e` itself is an error, the stream has already been in closed state,
    // so we don't care.
    //
    // If `e` itself does carry a payload, the end-of-stream marker is removed
    // by `TranslateMessage`, we don't care about it either.
    //
    // Therefore, we close the stream ourselves on be half of our user when
    // all of the above conditions are met.
    if (InlineEndOfStreamMarkerPresentInEmptyPayload(e)) {
      // Next read will see end-of-stream error.
      seen_inline_eos_ = true;

      // In this case we delay call to `cb` until the stream is closed.
      ctlr_->input_stream_->Close().Then(
          [this, cb = std::move(cb), e = std::move(e)]() mutable {
            TranslateAndCall(std::move(e), std::move(cb));
          });
      return;
    }

    TranslateAndCall(std::move(e), std::move(cb));
  };
  ctlr_->input_stream_->Read().Then(std::move(continuation));
}

template <class T>
void TypedInputStreamProvider<T>::Close(Function<void()> cb) {
  closed_ = true;  // Explicitly closed.

  if (!seen_inline_eos_) {
    ctlr_->input_stream_->Close().Then([cb = std::move(cb)] { cb(); });
  } else {
    // Otherwise the underlying stream has already been closed.
    cb();
  }

  if (!completion_notified_) {
    // FIXME: We notify the controller about the completion once the input
    // stream is closed, but we don't take output stream into consideration.
    // Therefore, it's possible the completion (user's "done") is called
    // even before he/she has finished writing all his/her requests. This
    // should be fixed.
    NotifyControllerCompletion(true);  // Assume success.
  }
}

template <class T>
bool TypedInputStreamProvider<T>::InlineEndOfStreamMarkerPresentInEmptyPayload(
    const Expected<MessagePtr, StreamError>& msg) {
  if (!msg) {
    return false;
  }
  return !!(msg->get()->GetType() & Message::Type::EndOfStream);
}

template <class T>
Expected<T, StreamError> TypedInputStreamProvider<T>::TranslateMessage(
    Expected<MessagePtr, StreamError>&& msg) {
  if (!msg) {
    return msg.error();
  }

  auto&& m = cast<ProtoMessage>(msg->get());
  if (m->msg_or_buffer.index() == 1) {
    auto&& payload = std::get<1>(m->msg_or_buffer).Get();
    if (payload) {
      // The actual message is not const, thus the cast should be safe.
      return std::move(*const_cast<T*>(flare::down_cast<T>(payload)));
    }  // Fall-through otherwise.
  } else {
    FLARE_CHECK_EQ(0, m->msg_or_buffer.index());
  }  // Fall-through.

  // No payload was attached to this EOS marker.
  if (!!(m->GetType() & Message::Type::EndOfStream)) {
    return StreamError::EndOfStream;
  } else {
    // It's a protocol error otherwise.
    return StreamError::IoError;
  }
}

template <class T>
void TypedInputStreamProvider<T>::TranslateAndCall(
    Expected<MessagePtr, StreamError>&& msg,
    Function<void(Expected<T, StreamError>)>&& cb) {
  auto translated = TranslateMessage(std::move(msg));
  if (!translated) {
    closed_ = true;  // Closed implicitly.
  }
  cb(std::move(translated));
}

template <class T>
void TypedInputStreamProvider<T>::NotifyControllerProgress(
    const Expected<MessagePtr, StreamError>& e) {
  if (e) {
    auto msg = cast<ProtoMessage>(e->get());
    ctlr_->NotifyStreamProgress(*msg->meta);

    // Reached end-of-stream.
    if (!!(msg->GetType() & Message::Type::EndOfStream)) {
      NotifyControllerCompletion(true);
    }
  } else {
    // End-of-stream marker is a successful completion.
    NotifyControllerCompletion(e.error() == StreamError::EndOfStream);
  }
}

template <class T>
void TypedInputStreamProvider<T>::NotifyControllerCompletion(bool success) {
  FLARE_CHECK(!completion_notified_);
  completion_notified_ = true;
  ctlr_->NotifyStreamCompletion(success);
}

}  // namespace flare::protobuf

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CONTROLLER_COMMON_INL_H_
