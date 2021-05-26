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

#ifndef FLARE_RPC_INTERNAL_BUFFERED_STREAM_PROVIDER_H_
#define FLARE_RPC_INTERNAL_BUFFERED_STREAM_PROVIDER_H_

#include <queue>
#include <utility>

#include "flare/base/logging.h"
#include "flare/fiber/timer.h"
#include "flare/rpc/internal/stream.h"

namespace flare::rpc::detail {

template <class T>
class BufferedStreamReaderProvider : public StreamReaderProvider<T> {
 public:
  // `on_close` is called before user's completion is called, `on_cleanup` is
  // called after user's callback.
  explicit BufferedStreamReaderProvider(std::size_t buffer_size,
                                        Function<void()> on_buffer_consumed,
                                        Function<void()> on_close,
                                        Function<void()> on_cleanup)
      : buffer_size_(buffer_size),
        on_buffer_available_(std::move(on_buffer_consumed)),
        on_close_(std::move(on_close)),
        on_cleanup_(std::move(on_cleanup)) {
    FLARE_CHECK(buffer_size > 0, "Be sane.");
  }

  ~BufferedStreamReaderProvider() {
    std::scoped_lock _(lock_);
    FLARE_CHECK(closed_, "You forgot to close the stream prior destroying it.");
    CHECK(!expiration_timer_);
  }

  void SetExpiration(
      std::chrono::steady_clock::time_point expires_at) override {
    std::scoped_lock _(lock_);
    if (closed_) {
      FLARE_VLOG(10, "Setting expiration on a closed stream has no effect.");
      return;
    }
    if (auto t = std::exchange(expiration_timer_, 0)) {
      fiber::KillTimer(t);
    }
    // Firing the timer multiple times won't hurt.
    expiration_timer_ =
        fiber::SetTimer(expires_at, [this, ref = RefPtr(ref_ptr, this)] {
          OnDataAvailable(StreamError::Timeout);
        });
  }

  // This method may not be called concurrently.
  //
  // Returns `true` if its internal buffer contains at least `buffer_size`
  // objects. (There can be even more objects if you keep calling this method,
  // which you should avoid.)
  void OnDataAvailable(Expected<T, StreamError> object) {
    RefPtr ref_self(ref_ptr, this);  // Keep us alive during the call. Otherwise
                                     // user's callback may destroy us.

    std::unique_lock lk(lock_);

    // Test if there's a reader first. If there isn't one, there's no point in
    // storing `object`.
    if (closed_) {
      FLARE_VLOG(
          10,
          "The stream has been closed, but the provider is keep feeding us "
          "with data. Dropped silently.");
      return;
    }

    // We don't check whether `object` evaluates to `true` here, it's checked in
    // `Peek` / `Read`.

    // Store the object first.
    objects_.push(std::move(object));

    // If there's a pending `Peek()`, satisfy it now.
    TryCompletePeekOperation(&lk);

    // If there's a pending `Read()`, complete it.
    TryCompleteReadOperation(&lk);
  }

  void Peek(Function<void(Expected<T, StreamError>*)> cb) override {
    RefPtr ref_self(ref_ptr, this);  // Keep us alive during the call. Otherwise
                                     // `cb` may destroy us.

    std::unique_lock lk(lock_);
    SanityCheck();

    // `TryCompletePeekOperation` might immediately reset `peek_cb_` if there's
    // data available.
    //
    // TODO(luobogao): This can be optimized.
    peek_cb_ = std::move(cb);
    TryCompletePeekOperation(&lk);
  }

  void Read(Function<void(Expected<T, StreamError>)> cb) override {
    RefPtr ref_self(ref_ptr, this);  // Keep us alive during the call. Otherwise
                                     // `cb` may destroy us.

    std::unique_lock lk(lock_);
    SanityCheck();

    // `TryCompleteReadOperation` might immediately reset `read_cb_` if there's
    // data available. TODO(luobogao): This can be optimized.
    read_cb_ = std::move(cb);
    TryCompleteReadOperation(&lk);
  }

  void Close(Function<void()> cb) override {
    RefPtr ref_self(ref_ptr, this);  // Keep us alive during the call. Otherwise
                                     // `cb` may destroy us.

    std::unique_lock lk(lock_);
    SanityCheck();
    NotifyClose(std::move(lk));

    cb();
    NotifyCleanup();
  }

 private:
  void SanityCheck() {
    FLARE_CHECK(!peek_cb_ && !read_cb_,
                "There's already a pending call to `Peek()` or `Read()`.");
    FLARE_CHECK(!closed_,
                "The stream is in an error state or has already been closed.");
  }

  // The caller is responsible for keep a ref-count on `*this` during the call.
  void NotifyClose(std::unique_lock<std::mutex> lk) {
    CHECK_GE(this->UnsafeRefCount(), 2);  // One kept by the caller, and one
                                          // by the user.
    auto cb = std::exchange(on_close_, nullptr);
    CHECK(!closed_);
    closed_ = true;

    if (auto t = std::exchange(expiration_timer_, 0)) {
      fiber::KillTimer(t);
    }

    lk.unlock();
    cb();
  }

  // The caller is responsible for keep a ref-count on `*this` during the call.
  //
  // The caller may not hold any lock when calling this method.
  void NotifyCleanup() {
    CHECK_GE(this->UnsafeRefCount(), 2);
    std::exchange(on_cleanup_, nullptr)();
  }

  // The caller is responsible for keep a ref-count on `*this` during the call.
  void TryCompletePeekOperation(std::unique_lock<std::mutex>* lk) {
    CHECK_GE(this->UnsafeRefCount(), 2);

    if (objects_.empty()) {
      return;  // Nothing to peek.
    }
    auto cb = std::exchange(peek_cb_, nullptr);
    if (!cb) {
      return;  // No pending peek.
    }

    // It can't be, otherwise `peek_cb_` should already been completed with an
    // error.
    FLARE_CHECK(!closed_,
                "The stream is in an error state and should be closed.");
    auto ptr = &objects_.front();
    bool need_close = !*ptr;
    if (need_close) {
      NotifyClose(std::move(*lk));
    }

    cb(ptr);
    if (need_close) {
      NotifyCleanup();
    }
  }

  // The caller is responsible for keep a ref-count on `*this` during the call.
  void TryCompleteReadOperation(std::unique_lock<std::mutex>* lk) {
    CHECK_GE(this->UnsafeRefCount(), 2);

    if (objects_.empty()) {
      return;  // Nothing to read.
    }
    auto cb = std::exchange(read_cb_, nullptr);
    if (!cb) {
      return;  // No reader.
    }

    FLARE_CHECK(!closed_,
                "The stream is in an error state and should be closed.");
    // After popping up this object, our internal buffer will become non-full.
    auto obj = std::move(objects_.front());
    bool need_close = !obj;
    objects_.pop();
    lk->unlock();

    on_buffer_available_();  // Called unconditionally.

    // If the stream is going to be closed anyway, we close it prior to call
    // user's completion.
    //
    // This should be no surprise to the user. By the time he / she reads an
    // erroneous value, he / she shouldn't access the stream anymore. Therefore
    // he / she won't be able to tell when the stream is closed, before or after
    // his / her callback is called.
    lk->lock();
    if (need_close) {
      // `cb` must have not closed the stream if this branch it taken, otherwise
      // the user is violating our using convention (by the time an error is
      // read, the stream must be treated as closed, and he / she may not closed
      // it again.).
      NotifyClose(std::move(*lk));
    } else {
      lk->unlock();
    }
    cb(std::move(obj));
    if (need_close) {
      NotifyCleanup();
    }
  }

 private:
  std::size_t buffer_size_;
  Function<void()> on_buffer_available_;
  Function<void()> on_close_;
  Function<void()> on_cleanup_;

  // Protects all variables below, they're used both by reader side (`Read()` /
  // `Peek()`) and writer side (`OnDataAvailable()`).
  std::mutex lock_;

  // Timeout timer.
  std::uint64_t expiration_timer_{};

  // Set when either `Close()` is called or `Read()` / `Peek()` failed.
  //
  // Note that it's set on call to `Read()` / `Peek()`, not on
  // `OnDataAvailable()`, this is required so that the user is aware when the
  // stream becomes broken.
  bool closed_ = false;

  // Pending `Peek()` / `Read()` operation.
  Function<void(Expected<T, StreamError>*)> peek_cb_;
  Function<void(Expected<T, StreamError>)> read_cb_;

  // Internal buffer.
  std::queue<Expected<T, StreamError>> objects_;
};

template <class T>
class BufferedStreamWriterProvider : public StreamWriterProvider<T> {
 public:
  // If there are less than `buffer_size` objects in buffer, writes are
  // completed immediately.
  //
  // Note that this raises a caveat: we actually notify the user his write is
  // successful, before we even tried. This may give the user a false sense of
  // success. For our use it's acceptable (since that even if the write indeed
  // succeeded, it's well possible to be lost on the network.), but it's not a
  // good design in general.
  explicit BufferedStreamWriterProvider(std::size_t buffer_size,
                                        Function<void(T)> writer,
                                        Function<void()> on_close,
                                        Function<void()> on_cleanup)
      : buffer_size_(buffer_size),
        writer_(std::move(writer)),
        on_close_(std::move(on_close)),
        on_cleanup_(std::move(on_cleanup)) {
    FLARE_CHECK(buffer_size > 0,
                "You should allow at least one uncompleted write (i.e., in "
                "which case buffering is totally disabled.).");
  }

  ~BufferedStreamWriterProvider() {
    std::scoped_lock _(lock_);
    FLARE_CHECK(closed_,
                "You forgot to close the stream prior to destroying it.");
    CHECK(!expiration_timer_);
  }

  void SetExpiration(
      std::chrono::steady_clock::time_point expires_at) override {
    std::scoped_lock _(lock_);
    if (closed_) {
      FLARE_VLOG(10, "Setting expiration on a closed stream has no effect.");
      return;
    }
    if (auto t = std::exchange(expiration_timer_, 0)) {
      fiber::KillTimer(t);
    }
    expiration_timer_ = fiber::SetTimer(
        expires_at,
        [this, ref = RefPtr(ref_ptr, this)] { OnWriteCompletion(false); });
  }

  // For successful callbacks, each write should be paired with a callback.
  //
  // For failure callbacks, one is enough for notifying all completing all
  // pending writes. (It's still allowed and preferable to pair callbacks with
  // writes in this case, but for the sake of simplicity, one is enough.).
  //
  // If the consumer is broken, the write is completed with failure as well.
  void OnWriteCompletion(bool success) {
    RefPtr self_ref(ref_ptr, this);
    std::unique_lock lk(lock_);

    if (broken_) {
      FLARE_VLOG(10, "The stream is broken but we've known it.");

      // Any pending operations should have been completed first time we knew
      // the stream was broken.
      CHECK(!write_cb_ && !last_write_cb_ && !close_cb_);
      return;
    }

    if (!success) {
      broken_ = true;
    }

    if (pending_writes_) {
      --pending_writes_;
      CHECK_LT(pending_writes_, buffer_size_);
    } else {
      // There must be one buffered write, otherwise we're receiving more
      // completion callbacks than the writes we issued. Therefore this branch
      // should never be taken.
      //
      // However, due to implementation issues, we blindly call this method with
      // `false` in case of underlying media broken (e.g., connection reset) or
      // timeout (@sa: `expiration_timer_`), which can reach here.
      //
      // We ignore it for now.
      CHECK(broken_);
      // Should already been called before.
      CHECK(!write_cb_ && !last_write_cb_ && !close_cb_);
      FLARE_VLOG(10, "Lower layer media broken?");
      return;
    }

    // We do not allow more than one pending operations, therefore at most one
    // of them can be set.
    CHECK_LE(!!write_cb_ + !!last_write_cb_ + !!close_cb_, 1);

    // If there's a write blocked, we unblock it.
    //
    // Note that since we're doing buffering, this `OnWriteCompletion` is,
    // actually, for an earlier `Write()`, not the one corresponding to
    // `write_cb_`. Therefore, this one is, again, an early completion of
    // `Write()`.
    if (auto cb = std::exchange(write_cb_, nullptr)) {
      CHECK(!last_write_cb_ && !close_cb_);
      lk.unlock();
      cb(success);
      // We have to return immediately, otherwise if the user freed us it `cb`,
      // we'll have a bad day.
      //
      // Given that `write_cb_` is present, neither `last_write_cb_` nor
      // `close_cb_` should be present, so there's nothing left for us to do
      // either.
      return;
    }  // Fallthrough, we need to check if `close_cb_` is present.

    // If the writing buffer is flushed and there's a "last write" callback,
    // fire it now.
    //
    // If there's an error, even if there are still buffered writes, we can
    // complete the "last write" earlier -- it should be completed with failure
    // anyway.
    CHECK(lk.owns_lock());
    if (!pending_writes_ || !success) {
      bool notify_close = !!last_write_cb_ || !!close_cb_;
      CHECK(!last_write_cb_ || !close_cb_);  // At most one of them can be set.
      auto last_write_cb = std::exchange(last_write_cb_, nullptr);
      auto close_cb = std::exchange(close_cb_, nullptr);

      if (notify_close) {
        NotifyClose(std::move(lk));
      }

      if (last_write_cb) {
        last_write_cb(success);
      }  // After calling `last_write_cb_`, the stream is treated as closed.
      if (close_cb) {
        close_cb();
      }
      if (notify_close) {
        NotifyCleanup();
      }
    }
  }

  void Write(T object, bool last, Function<void(bool)> cb) override {
    std::unique_lock lk(lock_);
    SanityCheck();

    // `closed_` is updated on `last_write_cb_` being called, don't update it
    // here.

    // If the stream has already broken, complete the write immediately.
    if (broken_) {
      // In case it's also a call to `WriteLast`, the stream should be closed.
      if (last) {
        NotifyClose(std::move(lk));
      }

      cb(false);
      if (last) {
        NotifyCleanup();
      }
      return;
    }

    ++pending_writes_;
    if (last) {
      // For last write, we must delay its completion until all writes have been
      // flushed.
      last_write_cb_ = std::move(cb);
      lk.unlock();
    } else {
      // Otherwise if there's still room, we can do an early completion.
      if (pending_writes_ < buffer_size_) {
        lk.unlock();
        cb(true);
      } else {
        write_cb_ = std::move(cb);
        lk.unlock();
      }
    }

    // Let's issue the write.
    CHECK(!lk.owns_lock());
    writer_(std::move(object));
  }

  void Close(Function<void()> cb) override {
    std::unique_lock lk(lock_);
    SanityCheck();

    if (pending_writes_ && !broken_) {
      // In case there are pending writes, we need to defer call to `on_close_`
      // in the same way as `last_write_cb_`.
      close_cb_ = std::move(cb);

      // NOTHING. `NotifyClose` will be called by `OnWriteCompletion`.
    } else {
      // Otherwise complete `Close()` right now.
      NotifyClose(std::move(lk));
      cb();
      NotifyCleanup();
    }
  }

 private:
  void SanityCheck() {
    FLARE_CHECK(
        !write_cb_ && !last_write_cb_ && !close_cb_,
        "Operation being performed on this stream has not completed yet.");
    FLARE_CHECK(!closed_, "The stream has been closed.");
  }

  void NotifyClose(std::unique_lock<std::mutex> lk) {
    CHECK_GE(this->UnsafeRefCount(), 2);

    CHECK(!write_cb_ && !last_write_cb_);
    auto cb = std::exchange(on_close_, nullptr);
    CHECK(!closed_);
    closed_ = true;

    if (auto t = std::exchange(expiration_timer_, 0)) {
      fiber::KillTimer(t);
    }
    lk.unlock();
    cb();
  }

  void NotifyCleanup() {
    CHECK_GE(this->UnsafeRefCount(), 2);
    std::exchange(on_cleanup_, nullptr)();
  }

 private:
  std::size_t buffer_size_;
  Function<void(T)> writer_;
  Function<void()> on_close_;
  Function<void()> on_cleanup_;

  std::mutex lock_;

  // Timeout timer.
  std::uint64_t expiration_timer_{};

  // This is not required for implementation, it's used for `CHECK`s.
  bool closed_ = false;

  // Unlike input stream, we don't care about being closed here, but rather,
  // what we care about is whether the stream is broken.
  bool broken_ = false;

  // Number of `Write()`s we completed before it indeed completes.
  std::size_t pending_writes_ = 0;
  Function<void(bool)> write_cb_;
  Function<void(bool)> last_write_cb_;
  Function<void()> close_cb_;
};

}  // namespace flare::rpc::detail

#endif  // FLARE_RPC_INTERNAL_BUFFERED_STREAM_PROVIDER_H_
