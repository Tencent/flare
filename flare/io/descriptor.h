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

#ifndef FLARE_IO_DESCRIPTOR_H_
#define FLARE_IO_DESCRIPTOR_H_

#include <sys/epoll.h>

#include <chrono>
#include <memory>
#include <string>

#include "thirdparty/googletest/gtest/gtest_prod.h"

#include "flare/base/align.h"
#include "flare/base/enum.h"
#include "flare/base/handle.h"
#include "flare/base/ref_ptr.h"
#include "flare/fiber/mutex.h"

namespace flare {

class EventLoop;

// We use `Descriptor` to describe file descriptors managed by `EventLoop`.
class alignas(hardware_destructive_interference_size) Descriptor
    : public RefCounted<Descriptor> {
 public:
  // Internally we use `Event::kXxx` and `EPOLLXXX` interchangeably, so don't
  // use customized value for the enumerators.
  enum class Event { Read = EPOLLIN, Write = EPOLLOUT };

  // `name` is used by `EventLoop` for writing logs.
  explicit Descriptor(Handle fd, Event events, const std::string& name = "");

  // It's the user's responsibility to ensure that the file descriptor has
  // been removed from `EventLoop`.
  virtual ~Descriptor();

  // Get the fd.
  int fd() const { return read_mostly_.fd.Get(); }

  // Returns the event loop we're associated with.
  EventLoop* GetEventLoop() const {
    return read_mostly_.ev.load(std::memory_order_relaxed);
  }

 protected:
  // Returned by `OnReadable` / `OnWritable` to notify the framework what have
  // been done by the implementation, or what should be done by the framework.
  enum class EventAction {
    // No special action will be taken.
    //
    // The implemented MUST saturate system's buffer before return.
    Ready,

    // The descriptor `Kill()`-ed itself in the callback.
    Leaving,

    // Suppress the event from happening in the future. It's the `Descriptor`'s
    // responsibility to reenable the event via `RestartReadIn()` /
    // `RestartWriteIn()`.
    Suppress
  };

  enum class CleanupReason {
    None,  // Placeholder, not actually used.
    HandshakeFailed,
    Disconnect,
    UserInitiated,
    Closing,
    Error
  };

  // The following callbacks are called in separate fibers. Different callbacks
  // can be called concurrently. Be prepared.

  // There's something to read.
  virtual EventAction OnReadable() = 0;

  // There's available buffer for writing.
  virtual EventAction OnWritable() = 0;

  // Something error happens. You should call `Kill()` in this method.
  virtual void OnError(int err) = 0;

  // The descriptor is in a quiescent state now. It has been removed from the
  // event loop, no concurrent call to descriptor callback is being / will be
  // made, and it can be destroyed immediately upon returning from this method.
  virtual void OnCleanup(CleanupReason reason) = 0;

  // These methods re-enable read / write events that was / will be disabled by
  // returning `Suppress` from `OnReadable` / `OnWritable`.
  //
  // It's safe to call these methods even before `OnReadable` / `OnWritable`
  // returns, in this case, returning `Suppress` has no effect.
  //
  // `after` allows you to specify a delay after how long will read / write be
  // re-enabled.
  void RestartReadIn(std::chrono::nanoseconds after);
  void RestartWriteIn(std::chrono::nanoseconds after);

  // Prevent events from happening. `OnCleanup()` will be called on completion.
  //
  // If the descriptor is killed for multiple times, only the first one take
  // effect.
  void Kill(CleanupReason reason);

  // Wait until `OnCleanup()` returns. `Kill()` must be called prior to calling
  // this method.
  void WaitForCleanup();

 private:
  FRIEND_TEST(Descriptor, ConcurrentRestartRead);
  friend class EventLoop;
  struct SeldomlyUsed;

  void SetEventLoop(EventLoop* ev) { read_mostly_.ev.store(ev); }
  const std::string& GetName() const;

  // The mask may contain other events that are not listed in `Event`.
  void SetEventMask(int mask) { read_mostly_.event_mask = mask; }
  int GetEventMask() const { return read_mostly_.event_mask; }

  void SetEnabled(bool f) { read_mostly_.enabled = f; }
  bool Enabled() const { return read_mostly_.enabled; }

  // Start one or more fibers to run events in `mask`.
  void FireEvents(int mask, std::uint64_t polled_at);
  void FireReadEvent(std::uint64_t fired_at);
  void FireWriteEvent(std::uint64_t fired_at);
  void FireErrorEvent(std::uint64_t fired_at);

  void SuppressReadAndClearReadEventCount();
  void SuppressWriteAndClearWriteEventCount();

  void RestartReadNow();
  void RestartWriteNow();

  void QueueCleanupCallbackCheck();

 private:
  // Number of `EPOLLIN` / `EPOLLOUT` events that have not been acknowledged (by
  // returning `Ready` from callbacks.).
  //
  // These are likely to be accessed soon after / before our ref-count is
  // mutated. So we place them as the first members (closer to ref-count).
  std::atomic<std::size_t> read_events_{}, write_events_{};

  // Set once the descriptor has been removed from the event loop.
  std::atomic<bool> cleanup_pending_{false};

  // Difference between number of calls to `RestartReadIn/Write()` and number of
  // `Suppress`-es returned from `OnReadable/Writable()`.
  //
  // These counters allow us to handle the case when `RestartReadIn/Write()` is
  // called before `OnReadable/Writable()` returns.
  //
  // Initialize to 1 if the corresponding event is enabled on construction, 0
  // otherwise. (@sa: constructor.)
  std::atomic<std::size_t> restart_read_count_, restart_write_count_;

  // Starts at another cache line. Fields below are not mutated frequently by
  // event-loop-related code path.
  struct alignas(hardware_destructive_interference_size) {
    Handle fd;
    std::atomic<EventLoop*> ev = nullptr;
    int event_mask;
    bool enabled{false};

    // These fields are seldomly used, so put them separately so as to save
    // precious cache space.
    std::unique_ptr<SeldomlyUsed> seldomly_used;
  } read_mostly_;
};

}  // namespace flare

namespace flare {

template <>
struct is_enum_bitmask_enabled<Descriptor::Event> {
  static constexpr bool value = true;
};

}  // namespace flare

#endif  // FLARE_IO_DESCRIPTOR_H_
