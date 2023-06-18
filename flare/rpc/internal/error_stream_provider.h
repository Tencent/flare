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

#ifndef FLARE_RPC_INTERNAL_ERROR_STREAM_PROVIDER_H_
#define FLARE_RPC_INTERNAL_ERROR_STREAM_PROVIDER_H_

#include "flare/rpc/internal/stream.h"

// Streams implemented via these providers always return error.
namespace flare::rpc::detail {

template <class T, StreamError Error = StreamError::IoError>
class ErrorStreamReaderProvider : public StreamReaderProvider<T> {
 public:
  void SetExpiration(
      std::chrono::steady_clock::time_point expires_at) override {
    // NOTHING.
  }

  void Peek(Function<void(Expected<T, StreamError>*)> cb) override {
    cb(&err_);
  }

  void Read(Function<void(Expected<T, StreamError>)> cb) override { cb(Error); }

  void Close(Function<void()> cb) override { cb(); }

 private:
  Expected<T, StreamError> err_{Error};
};

template <class T>
class ErrorStreamWriterProvider : public StreamWriterProvider<T> {
 public:
  void SetExpiration(
      std::chrono::steady_clock::time_point expires_at) override {
    // NOTHING.
  }

  void Write(T object, bool last, Function<void(bool)> cb) override {
    cb(false);
  }

  void Close(Function<void()> cb) override { cb(); }
};

}  // namespace flare::rpc::detail

#endif  // FLARE_RPC_INTERNAL_ERROR_STREAM_PROVIDER_H_
