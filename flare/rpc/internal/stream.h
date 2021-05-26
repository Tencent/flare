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

#ifndef FLARE_RPC_INTERNAL_STREAM_H_
#define FLARE_RPC_INTERNAL_STREAM_H_

#include <utility>

#include "flare/base/expected.h"
#include "flare/base/function.h"
#include "flare/base/future.h"
#include "flare/base/ref_ptr.h"
#include "flare/fiber/future.h"

namespace flare {

// Note that reading from / writing to stream classes defined here are NOT
// thread-safe.

namespace detail {

// I'm thinking of lifting this one into `future.h`. But I have not come up
// with a name satisfying enough yet.
//
// The name used here deviates from the naming convention of `MakeXxx` in
// `future.h`.
template <class... T, class F>
Future<T...> Futurized(F&& f) {
  Promise<T...> p;
  auto rf = p.GetFuture();
  std::forward<F>(f)(std::move(p));
  return rf;
}

}  // namespace detail

// Honestly use a centralized error code for StreamReaderProvider is not quite
// scalable..
//
// If desired, something like `int` or `std::error_code` may be used instead.
enum class StreamError {
  EndOfStream,
  IoError,
  Timeout,
  // ...
};

template <class T>
class StreamReaderProvider : public RefCounted<StreamReaderProvider<T>> {
 public:
  virtual ~StreamReaderProvider() = default;

  // Set timeout after which this stream should be considered broken.
  virtual void SetExpiration(
      std::chrono::steady_clock::time_point expires_at) = 0;

  // This method returns first object in the stream without removing it.
  //
  // Not all provider are required to implement this, for those who do not
  // support this method, it always calls `cb` with `nullptr`.
  virtual void Peek(Function<void(Expected<T, StreamError>*)> cb) = 0;

  // On failure, `cb` must be called with a failed "Expected". `Read()` itself
  // is not allowed to return failure. In this case, no more `Read()` is
  // allowed, and the stream is treated as closed.
  virtual void Read(Function<void(Expected<T, StreamError>)> cb) = 0;

  // IDK if there's a valid reason for this method to complete asynchronously.
  virtual void Close(Function<void()> cb) = 0;
};

template <class T>
class StreamWriterProvider : public RefCounted<StreamWriterProvider<T>> {
 public:
  virtual ~StreamWriterProvider() = default;

  // Set timeout after which this stream should be considered broken.
  virtual void SetExpiration(
      std::chrono::steady_clock::time_point expires_at) = 0;

  // `cb` is called when `Write` completes or buffered.
  //
  // Had a failure occurred, further writes must be completed with failure
  // immediately.
  //
  // If `last` is set, `cb` may only be called after all pending writes are
  // flushed. In this case, `Close()` is implied, and won't be called
  // explicitly.
  virtual void Write(T object, bool last, Function<void(bool)> cb) = 0;

  // Close the stream and flush any pending writes.
  virtual void Close(Function<void()> cb) = 0;
};

// Here was a pair of callback-based reader / writer, but I see little reason
// not to use future-based ones.

template <class T>
class AsyncStreamReader {
 public:
  AsyncStreamReader() = default;
  AsyncStreamReader(AsyncStreamReader&& ais) noexcept {
    std::swap(provider_, ais.provider_);
  }
  AsyncStreamReader& operator=(AsyncStreamReader&& ais) noexcept {
    FLARE_CHECK(!provider_);
    std::swap(provider_, ais.provider_);
    return *this;
  }

  explicit AsyncStreamReader(RefPtr<StreamReaderProvider<T>> provider)
      : provider_(std::move(provider)) {}

  // This method may be called at most once, and must be called before other
  // methods are called.
  void SetExpiration(std::chrono::steady_clock::time_point expires_at) {
    provider_->SetExpiration(expires_at);
  }

  // If the underlying provider does not support this operation, `nullptr` will
  // be returned.
  //
  // Until the `Future` returned is satisfied, no other methods on this object
  // may be called.
  //
  // Had an error occurred (except for retuning `nullptr`), i.e., the
  // `Expected<...>` returned evaluates to `false`, the stream should be treated
  // as closed and may not be touched except for destroying it.
  Future<Expected<T, StreamError>*> Peek() {
    return detail::Futurized<Expected<T, StreamError>*>([&](auto p) {
      provider_->Peek(
          [p = std::move(p)](auto&& e) mutable { p.SetValue(std::move(e)); });
    });
  }

  // Until the `Future` returned is satisfied, no other methods on this object
  // may be called.
  //
  // Had an error occurred, othe than destroying this object, you may not touch
  // it.
  Future<Expected<T, StreamError>> Read() {
    return detail::Futurized<Expected<T, StreamError>>([&](auto p) {
      provider_->Read(
          [p = std::move(p)](auto&& e) mutable { p.SetValue(std::move(e)); });
    });
  }

  Future<> Close() {
    return detail::Futurized<>([&](auto p) {
      provider_->Close([p = std::move(p)]() mutable { p.SetValue(); });
    });
  }

 private:
  RefPtr<StreamReaderProvider<T>> provider_;
};

template <class T>
class AsyncStreamWriter {
 public:
  AsyncStreamWriter() = default;
  AsyncStreamWriter(AsyncStreamWriter&& aos) noexcept {
    std::swap(provider_, aos.provider_);
  }
  AsyncStreamWriter& operator=(AsyncStreamWriter&& aos) noexcept {
    FLARE_CHECK(!provider_);
    std::swap(provider_, aos.provider_);
    return *this;
  }

  explicit AsyncStreamWriter(RefPtr<StreamWriterProvider<T>> provider)
      : provider_(std::move(provider)) {}

  // This method may be called at most once, and must be called before other
  // methods are called.
  void SetExpiration(std::chrono::steady_clock::time_point expires_at) {
    provider_->SetExpiration(expires_at);
  }

  // Until the `Future` returned is satisfied, you may not touch this object.
  //
  // Note that due to buffering, by the time a failure (if any) is returned,
  // multiple writes might have been lost. (@sa: `WriteLast`)
  //
  // Were a failure returned, further writes are all immediately completed with
  // failures, you should call `Close()` or `WriteLast()` to close the stream
  // (the latter convention can be convenient if you don't care about return
  // value of `Write()` and only want to check `WriteLast()`'s).
  Future<bool> Write(T object) {
    return detail::Futurized<bool>([&](auto p) {
      return provider_->Write(
          std::move(object), false,
          [p = std::move(p)](bool f) mutable { p.SetValue(f); });
    });
  }

  // It's not required to call this method for the last write, you can use
  // `Close()` after `Write()` to close the stream after you finished all
  // writes. However, calling this method for the last write can be a
  // performance gain in some cases.
  //
  // Until the `Future` returned is satisfied, you may not touch this object.
  //
  // If success is returned, it guaranteed all writes (including those issued
  // with `Write()`) has been successfully flushed out (but it can still be lost
  // due to network failure, etc.).
  //
  // You must treat the stream as closed after calling this method, any may not
  // touch the stream except for destroying it (of course, after the `Future` it
  // returned is satisfied.).
  Future<bool> WriteLast(T object) {
    return detail::Futurized<bool>([&](auto p) {
      return provider_->Write(
          std::move(object), true,
          [p = std::move(p)](bool f) mutable { p.SetValue(f); });
    });
  }

  // It's not specified whether pending writes are flushed or dropped after this
  // method returns. (@sa: `WriteLast`, which guarantees a flush.)
  //
  // Until the `Future` is satisfied, you may not touch this object.
  //
  // After this call, the stream is no longer usable, you must destroy it.
  Future<> Close() {
    return detail::Futurized<>([&](auto p) {
      provider_->Close([p = std::move(p)]() mutable { p.SetValue(); });
    });
  }

 private:
  RefPtr<StreamWriterProvider<T>> provider_;
};

// Implemented in terms of `AsyncStreamReader`. Block on `Future<>` internally.
template <class T>
class StreamReader {
 public:
  StreamReader() = default;
  StreamReader(StreamReader&&) = default;
  StreamReader& operator=(StreamReader&&) = default;

  explicit StreamReader(RefPtr<StreamReaderProvider<T>> provider)
      : ais_(std::move(provider)) {}

  void SetExpiration(std::chrono::steady_clock::time_point expires_at) {
    ais_.SetExpiration(expires_at);
  }

  Expected<T, StreamError>* Peek() { return fiber::BlockingGet(ais_.Peek()); }

  Expected<T, StreamError> Read() {
    // Our version. Not `gdt::`'s.
    //
    // TBH, this could be implemented without using `Future<>` (by calling
    // `StreamReaderProvider::Read()`). But that is a bit more complicated.
    return fiber::BlockingGet(ais_.Read());
  }

  void Close() { return fiber::BlockingGet(ais_.Close()); }

  constexpr explicit operator bool() const { return !!ais_; }

 private:
  AsyncStreamReader<T> ais_;
};

template <class T>
class StreamWriter {
 public:
  StreamWriter() = default;
  StreamWriter(StreamWriter&&) = default;
  StreamWriter& operator=(StreamWriter&&) = default;

  explicit StreamWriter(RefPtr<StreamWriterProvider<T>> provider)
      : aos_(std::move(provider)) {}

  void SetExpiration(std::chrono::steady_clock::time_point expires_at) {
    aos_.SetExpiration(expires_at);
  }

  bool Write(T object) {
    // Could be optimized slightly (by not using `Future<>`).
    return fiber::BlockingGet(aos_.Write(std::move(object)));
  }

  bool WriteLast(T object) {
    // Could be optimized slightly (by not using `Future<>`).
    return fiber::BlockingGet(aos_.WriteLast(std::move(object)));
  }

  void Close() { return fiber::BlockingGet(aos_.Close()); }

  constexpr explicit operator bool() const { return !!aos_; }

 private:
  AsyncStreamWriter<T> aos_;
};

}  // namespace flare

#endif  // FLARE_RPC_INTERNAL_STREAM_H_
