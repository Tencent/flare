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

#include "flare/io/descriptor.h"

#include <chrono>
#include <memory>
#include <utility>

#include "flare/base/align.h"
#include "flare/base/internal/memory_barrier.h"
#include "flare/base/logging.h"
#include "flare/base/string.h"
#include "flare/fiber/condition_variable.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/this_fiber.h"
#include "flare/fiber/timer.h"
#include "flare/io/event_loop.h"
#include "flare/io/util/socket.h"

using namespace std::literals;

namespace flare {

ExposedMetrics<std::uint64_t, flare::detail::TscToDuration<std::uint64_t>>
    read_event_fire_to_completion_latency(
        "flare/io/latency/event_fire_to_completion/read");
ExposedMetrics<std::uint64_t, flare::detail::TscToDuration<std::uint64_t>>
    write_event_fire_to_completion_latency(
        "flare/io/latency/event_fire_to_completion/write");
ExposedMetrics<std::uint64_t, flare::detail::TscToDuration<std::uint64_t>>
    error_event_fire_to_completion_latency(
        "flare/io/latency/event_fire_to_completion/error");

struct Descriptor::SeldomlyUsed {
  std::string name;

  std::atomic<bool> cleanup_queued{false};

  // Incremented whenever `EPOLLERR` is seen.
  //
  // FIXME: Can we really see more than one `EPOLLERR` in practice?
  std::atomic<std::size_t> error_events{};
  std::atomic<bool> error_seen{};  // Prevent multiple `EPOLLERR`s.

  // Set to non-`None` once a cleanup event is pending. If multiple events
  // triggered cleanup (e.g., an error occurred and the descriptor is
  // concurrently being removed from the `EventLoop`), the first one wins.
  std::atomic<CleanupReason> cleanup_reason{CleanupReason::None};

  // For implementing `WaitForCleanup()`.
  fiber::Mutex cleanup_lk;
  fiber::ConditionVariable cleanup_cv;
  bool cleanup_completed{false};
};

Descriptor::Descriptor(Handle fd, Event events, const std::string& name)
    : read_mostly_{.fd = std::move(fd),
                   .event_mask = static_cast<int>(events),
                   .seldomly_used = std::make_unique<SeldomlyUsed>()} {
  static_assert(offsetof(Descriptor, read_mostly_) ==
                hardware_destructive_interference_size);
  restart_read_count_ = (read_mostly_.event_mask & EPOLLIN) ? 1 : 0;
  restart_write_count_ = (read_mostly_.event_mask & EPOLLOUT) ? 1 : 0;
  if (name.empty()) {
    read_mostly_.seldomly_used->name = Format("{}", fmt::ptr(this));
  } else {
    read_mostly_.seldomly_used->name = Format("{} @ {}", name, fmt::ptr(this));
  }
}

Descriptor::~Descriptor() {
  FLARE_CHECK(!Enabled(),
              "Descriptor {} is still associated with event loop {} when it's "
              "destructed.",
              fmt::ptr(this), fmt::ptr(GetEventLoop()));
}

void Descriptor::RestartReadIn(std::chrono::nanoseconds after) {
  if (after != 0ns) {
    // This keeps us alive until our task is called.
    auto ref = RefPtr(ref_ptr, this);

    auto timer = fiber::internal::CreateTimer(
        ReadSteadyClock() + after, [this, ref = std::move(ref)](auto timer_id) {
          fiber::internal::KillTimer(timer_id);
          RestartReadNow();
        });
    fiber::internal::EnableTimer(timer);
  } else {
    RestartReadNow();
  }
}

void Descriptor::RestartWriteIn(std::chrono::nanoseconds after) {
  if (after != 0ns) {
    auto timer = fiber::internal::CreateTimer(
        ReadSteadyClock() + after,
        [this, ref = RefPtr(ref_ptr, this)](auto timer_id) {
          fiber::internal::KillTimer(timer_id);
          RestartWriteNow();
        });
    fiber::internal::EnableTimer(timer);
  } else {
    RestartWriteNow();
  }
}

void Descriptor::Kill(CleanupReason reason) {
  FLARE_CHECK(reason != CleanupReason::None);
  auto expected = CleanupReason::None;
  if (read_mostly_.seldomly_used->cleanup_reason.compare_exchange_strong(
          expected, reason, std::memory_order_relaxed)) {
    GetEventLoop()->AddTask([this, ref = RefPtr(ref_ptr, this)] {
      GetEventLoop()->DisableDescriptor(this);
      cleanup_pending_.store(true, std::memory_order_relaxed);
      // From now on, no more call to `FireEvents()` will be made.

      QueueCleanupCallbackCheck();
    });
  }
}

void Descriptor::WaitForCleanup() {
  std::unique_lock lk(read_mostly_.seldomly_used->cleanup_lk);
  read_mostly_.seldomly_used->cleanup_cv.wait(
      lk, [this] { return read_mostly_.seldomly_used->cleanup_completed; });
}

const std::string& Descriptor::GetName() const {
  return read_mostly_.seldomly_used->name;
}

void Descriptor::FireEvents(int mask, std::uint64_t polled_at) {
  if (FLARE_UNLIKELY(mask & EPOLLERR)) {
    // `EPOLLERR` is handled first. In this case other events are ignored. You
    // don't want to read from / write to a file descriptor in error state.
    //
    // @sa: https://stackoverflow.com/a/37079607
    FireErrorEvent(polled_at);
    return;
  }
  if (mask & EPOLLIN) {
    // TODO(luobogao): For the moment `EPOLLRDHUP` is not enabled in
    // `EventLoop`.
    FireReadEvent(polled_at);
  }
  if (mask & EPOLLOUT) {
    FireWriteEvent(polled_at);
  }
}

void Descriptor::FireReadEvent(std::uint64_t fired_at) {
  ScopedDeferred _([&] {
    read_event_fire_to_completion_latency->Report(
        TscElapsed(fired_at, ReadTsc()));
  });

  // Acquiring here guarantees thatever had done by prior call to `OnReadable()`
  // is visible to us (as the prior call ends with a releasing store to the
  // event count.).
  if (read_events_.fetch_add(1, std::memory_order_acquire) == 0) {
    // `read_events_` was 0, so no fiber was calling `OnReadable`. Let's call
    // it then.
    fiber::internal::StartFiberDetached([this] {
      // The reference we keep here keeps us alive until we leave.
      //
      // The reason why we can be destroyed while executing is that if someone
      // else is executing `QueueCleanupCallbackCheck()`, it only waits until
      // event counters (in this case, `read_event_`) reaches 0.
      //
      // If we're delayed long enough, it's possible that after we decremented
      // `read_event_` to zero, but before doing the rest job, we're destroyed.
      //
      // Need to hold a reference each time read event need to be handled is
      // unfortunate, but if we add yet another counter for counting outstanding
      // fibers like us, the overhead is the same (in both case it's a atomic
      // increment).
      RefPtr self_ref(ref_ptr, this);

      do {
        auto rc = OnReadable();
        if (FLARE_LIKELY(rc == EventAction::Ready)) {
          continue;
        } else if (FLARE_UNLIKELY(rc == EventAction::Leaving)) {
          FLARE_CHECK(read_mostly_.seldomly_used->cleanup_reason.load(
                          std::memory_order_relaxed) != CleanupReason::None,
                      "Did you forget to call `Kill()`?");
          // We can only reset the counter in event loop's context.
          //
          // As `Kill()` as been called, by the time our task is run by the
          // event loop, this descriptor has been disabled, and no more call to
          // `FireReadEvent()` (the only one who increments `read_events_`), so
          // it's safe to reset the counter.
          //
          // Meanwhile, `QueueCleanupCallbackCheck()` is called after our task,
          // by the time it checked the counter, it's zero as expected.
          GetEventLoop()->AddTask([this] {
            read_events_.store(0, std::memory_order_relaxed);
            QueueCleanupCallbackCheck();
          });
          break;
        } else if (rc == EventAction::Suppress) {
          SuppressReadAndClearReadEventCount();

          // CAUTION: Here we break out before `read_events_` is drained. This
          // is safe though, as `SuppressReadAndClearReadEventCount()` will
          // reset `read_events_` to zero after is has disabled the event.
          break;
        }
      } while (read_events_.fetch_sub(1, std::memory_order_release) !=
               1);  // Loop until we decremented `read_events_` to zero. If more
                    // data has come before `OnReadable()` returns, the loop
                    // condition will hold.
      QueueCleanupCallbackCheck();
    });
  }  // Otherwise someone else is calling `OnReadable`. Nothing to do then.
}

void Descriptor::FireWriteEvent(std::uint64_t fired_at) {
  ScopedDeferred _([&] {
    write_event_fire_to_completion_latency->Report(
        TscElapsed(fired_at, ReadTsc()));
  });

  if (write_events_.fetch_add(1, std::memory_order_acquire) == 0) {
    fiber::internal::StartFiberDetached([this] {
      RefPtr self_ref(ref_ptr, this);  // @sa: `FireReadEvent()`.
      do {
        auto rc = OnWritable();
        if (FLARE_LIKELY(rc == EventAction::Ready)) {
          continue;
        } else if (FLARE_UNLIKELY(rc == EventAction::Leaving)) {
          FLARE_CHECK(read_mostly_.seldomly_used->cleanup_reason.load(
                          std::memory_order_relaxed) != CleanupReason::None,
                      "Did you forget to call `Kill()`?");
          GetEventLoop()->AddTask([this] {
            write_events_.store(0, std::memory_order_relaxed);
            QueueCleanupCallbackCheck();
          });
          break;
        } else if (rc == EventAction::Suppress) {
          SuppressWriteAndClearWriteEventCount();
          break;  // `write_events_` can be non-zero. (@sa: `FireReadEvent` for
                  // more explanation.)
        }
      } while (write_events_.fetch_sub(1, std::memory_order_release) !=
               1);  // Loop until `write_events_` reaches zero.
      QueueCleanupCallbackCheck();
    });
  }
}

void Descriptor::FireErrorEvent(std::uint64_t fired_at) {
  ScopedDeferred _([&] {
    error_event_fire_to_completion_latency->Report(
        TscElapsed(fired_at, ReadTsc()));
  });

  if (read_mostly_.seldomly_used->error_seen.exchange(
          true, std::memory_order_relaxed)) {
    FLARE_VLOG(10, "Unexpected: Multiple `EPOLLERR` received.");
    return;
  }

  if (read_mostly_.seldomly_used->error_events.fetch_add(
          1, std::memory_order_acquire) == 0) {
    fiber::internal::StartFiberDetached([this] {
      RefPtr self_ref(ref_ptr, this);  // @sa: `FireReadEvent()`.
      OnError(io::util::GetSocketError(fd()));
      FLARE_CHECK_EQ(read_mostly_.seldomly_used->error_events.fetch_sub(
                         1, std::memory_order_release),
                     1);
      QueueCleanupCallbackCheck();
    });
  } else {
    FLARE_CHECK(!"Unexpected");
  }
}

void Descriptor::SuppressReadAndClearReadEventCount() {
  // This must be done in `EventLoop`. Otherwise order of calls to
  // `RearmDescriptor` is nondeterministic.
  GetEventLoop()->AddTask([this, ref = RefPtr(ref_ptr, this)] {
    // We reset `read_events_` to zero first, as it's left non-zero when we
    // leave `FireReadEvent()`.
    //
    // No race should occur. `FireReadEvent()` itself is called in `EventLoop`
    // (where we're running), so it can't race with us. The only other one who
    // can change `read_events_` is the fiber who called us, and it should
    // have break out the `while` loop immediately after calling us without
    // touching `read_events_` any more.
    //
    // So we're safe here.
    read_events_.store(0, std::memory_order_release);

    // This is needed in case the descriptor is going to leave and its
    // `OnReadable()` returns `Suppress`.
    QueueCleanupCallbackCheck();

    if (Enabled()) {
      auto reached =
          restart_read_count_.fetch_sub(1, std::memory_order_relaxed) - 1;
      // If `reached` (i.e., `restart_read_count_` after decrement) reaches 0,
      // we're earlier than `RestartReadIn()`.

      // FIXME: For the moment there can be more `RestartRead` than read
      // suppression. This is caused by streaming RPC. `StreamIoAdaptor`
      // triggers a `RestartRead()` each time its internal buffer dropped down
      // below its buffer limit, but `Suppress` is only returned when the
      // system's buffer has been drained. While we're draining system's buffer,
      // `StreamIoAdaptor`'s internal buffer can reach and drop down from its
      // buffer limit several times since we keep feeding it before we finally
      // return `Suppress`.
      //
      // Let's see if we can return `Suppress` immediately when
      // `StreamIoAdaptor`'s internal buffer is filled up.

      FLARE_CHECK_NE(reached, -1);
      FLARE_CHECK(GetEventMask() & EPOLLIN);  // Were `EPOLLIN` to be removed,
                                              // it's us who remove it.
      if (reached == 0) {
        SetEventMask(GetEventMask() & ~EPOLLIN);
        GetEventLoop()->RearmDescriptor(this);
      } else {
        // Otherwise things get tricky. In this case we left system's buffer
        // un-drained, and `RestartRead` happens before us. From system's
        // perspective, this scenario just looks like we haven't drained its
        // buffer yet, so it won't return a `EPOLLIN` again.
        //
        // We have to either emulate one or remove and re-add the descriptor to
        // the event loop in this case.
        FireEvents(EPOLLIN, ReadTsc() /* Not quite precise. */);
      }
    }  // The descriptor is leaving otherwise, nothing to do.
  });
}

void Descriptor::SuppressWriteAndClearWriteEventCount() {
  GetEventLoop()->AddTask([this, ref = RefPtr(ref_ptr, this)] {
    // Largely the same as `SuppressReadAndClearReadEventCount()`.
    write_events_.store(0, std::memory_order_relaxed);
    QueueCleanupCallbackCheck();

    if (Enabled()) {
      auto reached =
          restart_write_count_.fetch_sub(1, std::memory_order_relaxed) - 1;

      FLARE_CHECK(reached == 0 || reached == 1,
                  "Unexpected restart-write count: {}", reached);
      FLARE_CHECK(GetEventMask() & EPOLLOUT);
      if (reached == 0) {
        SetEventMask(GetEventMask() & ~EPOLLOUT);
        GetEventLoop()->RearmDescriptor(this);
      } else {
        FireEvents(EPOLLOUT, ReadTsc());
      }
    }  // The descriptor is leaving otherwise, nothing to do.
  });
}

void Descriptor::RestartReadNow() {
  GetEventLoop()->AddTask([this, ref = RefPtr(ref_ptr, this)] {
    if (Enabled()) {
      auto count = restart_read_count_.fetch_add(1, std::memory_order_relaxed);

      // `count` is 0 if `Suppress` was returned from `OnReadable`. `count` is
      // 1 if we're called before `Suppress` is returned. Any other values are
      // unexpected.
      //
      // NOT `CHECK`-ed, though. @sa: `SuppressReadAndClearReadEventCount()`.

      if (count == 0) {  // We changed it from 0 to 1.
        FLARE_CHECK_EQ(GetEventMask() & EPOLLIN, 0);
        SetEventMask(GetEventMask() | EPOLLIN);
        GetEventLoop()->RearmDescriptor(this);
      }  // Otherwise `Suppress` will see `restart_read_count_` non-zero, and
         // deal with it properly.
    }
  });
}

void Descriptor::RestartWriteNow() {
  GetEventLoop()->AddTask([this, ref = RefPtr(ref_ptr, this)] {
    if (Enabled()) {
      auto count = restart_write_count_.fetch_add(1, std::memory_order_relaxed);

      FLARE_CHECK(count == 0 || count == 1,
                  "Unexpected restart-write count: {}", count);
      if (count == 0) {
        FLARE_CHECK_EQ(GetEventMask() & EPOLLOUT, 0);
        SetEventMask(GetEventMask() | EPOLLOUT);
        GetEventLoop()->RearmDescriptor(this);
      }
    }
  });
}

void Descriptor::QueueCleanupCallbackCheck() {
  // Full barrier, hurt performance.
  //
  // Here we need it to guarantee that:
  //
  // - For `Kill()`, it's preceding store to `cleanup_pending_` cannot be
  //   reordered after reading `xxx_events_`;
  //
  // - For `FireXxxEvent()`, it's load of `cleanup_pending_` cannot be reordered
  //   before its store to `xxx_events_`.
  //
  // Either case, reordering leads to falsely treating the descriptor in use.
  internal::MemoryBarrier();

  if (FLARE_LIKELY(!cleanup_pending_.load(std::memory_order_relaxed))) {
    return;
  }

  // Given that the descriptor is removed from the event loop prior to setting
  // `cleanup_pending_` to zero, by reaching here we can be sure that no more
  // `FireEvents()` will be called. This in turns guarantees us that
  // `xxx_events_` can only be decremented (they're solely incremented by
  // `FireEvents()`.).
  //
  // So we check if all `xxx_events_` reached zero, and fire `OnCleanup()` if
  // they did.
  if (read_events_.load(std::memory_order_relaxed) == 0 &&
      write_events_.load(std::memory_order_relaxed) == 0 &&
      read_mostly_.seldomly_used->error_events.load(
          std::memory_order_relaxed) == 0) {
    // Consider queue a call to `OnCleanup()` then.
    if (!read_mostly_.seldomly_used->cleanup_queued.exchange(
            true, std::memory_order_release)) {
      // No need to take a reference to us, `OnCleanup()` has not been called.
      GetEventLoop()->AddTask([this] {
        // The load below acts as a fence (paired with `exchange` above). (But
        // does it make sense?)
        (void)read_mostly_.seldomly_used->cleanup_queued.load(
            std::memory_order_acquire);

        // They can't have changed.
        FLARE_CHECK_EQ(read_events_.load(std::memory_order_relaxed), 0);
        FLARE_CHECK_EQ(write_events_.load(std::memory_order_relaxed), 0);
        FLARE_CHECK_EQ(read_mostly_.seldomly_used->error_events.load(
                           std::memory_order_relaxed),
                       0);

        // Hold a reference to us. `DetachDescriptor` decrements reference
        // count, which might be the last one.
        RefPtr self_ref(ref_ptr, this);

        // Detach the descriptor and call user's `OnCleanup`.
        GetEventLoop()->DetachDescriptor(this);
        OnCleanup(read_mostly_.seldomly_used->cleanup_reason.load(
            std::memory_order_relaxed));

        // Wake up any waiters on `OnCleanup()`.
        std::scoped_lock _(read_mostly_.seldomly_used->cleanup_lk);
        read_mostly_.seldomly_used->cleanup_completed = true;
        read_mostly_.seldomly_used->cleanup_cv.notify_one();
      });
    }
  }
}

}  // namespace flare
