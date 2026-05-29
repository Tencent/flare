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

#include "flare/io/event_loop.h"

#include <memory>
#include <utility>
#include <vector>

#include "gflags/gflags.h"

#include "flare/base/exposed_var.h"
#include "flare/base/logging.h"
#include "flare/base/random.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/fiber_local.h"
#include "flare/fiber/latch.h"
#include "flare/fiber/runtime.h"
#include "flare/fiber/this_fiber.h"
#include "flare/io/descriptor.h"
#include "flare/io/detail/eintr_safe.h"
#include "flare/io/detail/poller.h"
#include "flare/io/detail/timed_call.h"
#include "flare/io/detail/watchdog.h"
#include "flare/io/util/socket.h"

using namespace std::literals;

DEFINE_bool(flare_enable_watchdog, true,
            "Periodically test if event loops are still responsive enough, and "
            "crash the program if it's not.");
DEFINE_int32(flare_event_loop_per_scheduling_group, 1,
             "Number of event loops per scheduling group. Normally the default "
             "setting is sufficient.");

namespace flare {

namespace {

FiberLocal<EventLoop*> current_event_loop;

constexpr auto kPollerErrorMask = io::detail::kPollerError;
constexpr auto kExtraPollerFlags = io::detail::kPollerET;

struct EventLoopWorker {
  std::unique_ptr<EventLoop> event_loop;
  Fiber fiber;
};

// (Scheduling group index, event loop index).
std::vector<std::vector<EventLoopWorker>> event_loop_workers;

// Watchdog periodically checks if our `EventLoop`s are still responsive enough,
// and crash the whole program if they're not.
io::detail::Watchdog watchdog;

ExposedMetrics<std::uint64_t, flare::detail::TscToDuration<std::uint64_t>>
    run_event_handlers_latency("flare/io/latency/run_event_handlers");
ExposedMetrics<std::uint64_t, flare::detail::TscToDuration<std::uint64_t>>
    run_user_tasks_latency("flare/io/latency/run_user_tasks");
ExposedMetrics<std::uint64_t> events_per_poll("flare/io/events_per_poll");
ExposedCounter<std::uint64_t> user_tasks_run("flare/io/user_tasks_run");

// Shamelessly copied from https://stackoverflow.com/a/57556517
std::uint32_t HashFd(int fd) {
  auto xorshift = [](std::uint64_t n, std::uint64_t i) { return n ^ (n >> i); };
  uint64_t p = 0x5555555555555555;       // pattern of alternating 0 and 1
  uint64_t c = 17316035218449499591ull;  // random uneven integer constant;
  return c * xorshift(p * xorshift(fd, 32), 32);
}

}  // namespace

EventLoop::EventLoop() {
  poller_ = io::detail::CreatePoller();

  // `EventLoopNotifier` is different in that its `OnReadable` must be called
  // synchronously (to avoid wake-up loss), and hence must be handled
  // individually.
  poller_->Add(notifier_.fd(),
               io::detail::kPollerRead | io::detail::kPollerError,
               static_cast<void*>(&notifier_));
}

EventLoop::~EventLoop() {
  // Does not make much sense as both `notifier_.fd()` and the poller are
  // going to be closed anyway.
  poller_->Remove(notifier_.fd());
}

void EventLoop::AttachDescriptor(Descriptor* desc, bool enabled) {
  desc->Ref();
  desc->SetEventMask(desc->GetEventMask() | kPollerErrorMask |
                     kExtraPollerFlags);

  // We must call `SetEventLoop()` **before** adding the descriptor into the
  // event loop. Otherwise the descriptor may get a `nullptr` from
  // `GetEventLoop()` in its `OnXxx` callback.
  desc->SetEventLoop(this);
  // `desc->Enabled()` was initialized to `false`, do NOT initialize it here.
  if (enabled) {
    EnableDescriptor(desc);
  }
}

void EventLoop::EnableDescriptor(Descriptor* desc) {
  FLARE_CHECK(!desc->Enabled(), "The descriptor has already been enabled.");
  desc->SetEnabled(true);
  auto mask = desc->GetEventMask();
  poller_->Add(desc->fd(), mask, static_cast<void*>(desc));
  FLARE_VLOG(20, "Added descriptor [{}] with event mask [{}].", desc->GetName(),
             mask);
}

void EventLoop::RearmDescriptor(Descriptor* desc) {
  FLARE_CHECK(desc->Enabled(), "The descriptor is not enabled.");
  auto mask = desc->GetEventMask() | kPollerErrorMask | kExtraPollerFlags;
  FLARE_VLOG(20, "Rearming descriptor [{}] with event mask [{}].",
             desc->GetName(), mask);
  poller_->Modify(desc->fd(), mask, static_cast<void*>(desc));
}

void EventLoop::DisableDescriptor(Descriptor* desc) {
  FLARE_CHECK_EQ(desc->GetEventLoop(), this);
  FLARE_CHECK_EQ(EventLoop::Current(), this,
                 "This method must be called in event loop's context.");
  FLARE_CHECK(desc->Enabled(), "The descriptor is not enabled.");
  poller_->Remove(desc->fd());
  FLARE_VLOG(20, "Removed descriptor [{}].", desc->GetName());
  desc->SetEnabled(false);
}

void EventLoop::DetachDescriptor(Descriptor* desc) {
  FLARE_CHECK_EQ(desc->GetEventLoop(), this);
  FLARE_CHECK_EQ(EventLoop::Current(), this,
                 "This method must be called in event loop's context.");
  FLARE_CHECK(!desc->Enabled(),
              "The descriptor must be disabled before calling this method.");

  desc->Deref();
}

void EventLoop::AddTask(Function<void()>&& cb) {
  {
    std::scoped_lock lk(tasks_lock_);
    tasks_.push_back(std::move(cb));
  }
  notifier_.Notify();  // Wake up the event loop to run our callback.
}

void EventLoop::Barrier() {
  fiber::Latch l(1);
  AddTask([&l] { l.count_down(); });
  l.wait();
}

void EventLoop::Run() {
  *current_event_loop = this;

  while (!exiting_.load(std::memory_order_relaxed)) {
    // May block if there's no event pending.
    //
    // Could be woke up if `notifier_` fires, or new event on fds appears.
    //
    // Only returns once all events (including those deferred) are handled.
    WaitAndRunEvents(5ms);

    // Use's callbacks should be run after `Descriptor`'s callbacks are run.
    // @sa: Comments on `AddTask`.
    io::detail::TimedCall([&] { RunUserTasks(); }, 5ms, "RunUserTasks()");

    // This one helps performance under load.
    //
    // The reason is that the event loop in itself is unlikely to saturate a
    // pthread worker. If we `Yield()` here, we can donate CPU resources
    // allocated to this pthread worker to other (presumably just created)
    // fibers. Otherwise this pthread worker would block on `epoll_wait`, which,
    // in the case when we do not allocate spare workers, wastes pthread
    // workers.
    this_fiber::Yield();
  }

#ifndef NDEBUG
  std::scoped_lock tasks_lk(tasks_lock_);
  FLARE_CHECK(tasks_.empty(),
              "You likely tried posting tasks after `Stop()` is called.");
#endif

  *current_event_loop = nullptr;  // Use a scope guard here would be better.
}

void EventLoop::Stop() {
  // NOTHING.
}

void EventLoop::Join() {
  // Let the event loop go.
  exiting_ = true;

  // It's the caller's responsibility to join on the fiber running the event
  // loop (as it's the caller who created the fiber.).
}

EventLoop* EventLoop::Current() { return *current_event_loop; }

void EventLoop::WaitAndRunEvents(std::chrono::milliseconds wait_for) {
  constexpr auto kDescriptorsPerLoop = 128;
  io::detail::PollerEvent evs[kDescriptorsPerLoop];
  auto nfds = poller_->Wait(evs, std::size(evs), wait_for / 1ms);
  FLARE_PCHECK(nfds >= 0, "Unexpected: poller wait failed.");

  // Run event handlers.
  io::detail::TimedCall([&] { RunEventHandlers(evs, evs + nfds); }, 5ms,
                        "RunEventHandlers()");
}

void EventLoop::RunUserTasks() {
  std::list<Function<void()>> cbs;
  {
    std::scoped_lock lk(tasks_lock_);
    cbs.swap(tasks_);
  }

  if (!cbs.empty()) {
    ScopedDeferred _([start_tsc = ReadTsc()] {
      run_user_tasks_latency->Report(TscElapsed(start_tsc, ReadTsc()));
    });
    // We don't expect too many tasks in the queue. Neither do we expect the
    // tasks to run too long.
    while (!cbs.empty()) {
      io::detail::TimedCall([&] { cbs.front()(); }, 5ms, "User's task");
      user_tasks_run->Increment();
      cbs.pop_front();
    }
  }
}

void EventLoop::RunEventHandlers(io::detail::PollerEvent* begin,
                                 io::detail::PollerEvent* end) {
  auto start_tsc = ReadTsc();
  ScopedDeferred _([&] {
    run_event_handlers_latency->Report(TscElapsed(start_tsc, ReadTsc()));
  });
  events_per_poll->Report(end - begin);

  while (begin != end) {
    // FIXME: This `if` is ugly.
    if (FLARE_UNLIKELY(begin->user_data == static_cast<void*>(&notifier_))) {
      FLARE_CHECK((begin->events & io::detail::kPollerError) == 0,
                  "Unexpected error on event loop notifier.");
      notifier_.Reset();
      ++begin;
      continue;
    }

    auto desc = reinterpret_cast<Descriptor*>(begin->user_data);
    FLARE_CHECK(desc);
    desc->FireEvents(begin->events, start_tsc);
    ++begin;
  }
}

void StartAllEventLoops() {
  Latch all_started(fiber::GetSchedulingGroupCount() *
                    FLAGS_flare_event_loop_per_scheduling_group);

  event_loop_workers.resize(fiber::GetSchedulingGroupCount());
  for (std::size_t sgi = 0; sgi != event_loop_workers.size(); ++sgi) {
    event_loop_workers[sgi].resize(FLAGS_flare_event_loop_per_scheduling_group);
    for (std::size_t eli = 0;
         eli != FLAGS_flare_event_loop_per_scheduling_group; ++eli) {
      auto&& elw = event_loop_workers[sgi][eli];
      auto start_cb = [&all_started, elwp = &elw] {
        elwp->event_loop->AddTask([&all_started] { all_started.count_down(); });
        elwp->event_loop->Run();
      };

      // FIXME: Need we allocate `EventLoop` in its scheduling group instead?
      elw.event_loop = std::make_unique<EventLoop>();
      // TODO(luobogao): Give a name to this fiber.
      elw.fiber = Fiber(Fiber::Attributes{.scheduling_group = sgi,
                                          .scheduling_group_local = true},
                        start_cb);
      watchdog.AddEventLoop(elw.event_loop.get());
    }
  }
  all_started.wait();
  if (FLAGS_flare_enable_watchdog) {
    watchdog.Start();
  }
}

EventLoop* GetGlobalEventLoop(std::size_t scheduling_group, int fd) {
  FLARE_CHECK(fd != 0 && fd != -1,
              "You're likely passing in a fd got from calling `Get()` on an "
              "invalid `Handle`.");
  if (fd == -2) {
    fd = Random<int>();
  }
  FLARE_CHECK_LT(scheduling_group, event_loop_workers.size());

  // TODO(luobogao): Let's see if the hash works well.
  auto eli = HashFd(fd) % FLAGS_flare_event_loop_per_scheduling_group;
  auto&& ptr = event_loop_workers[scheduling_group][eli].event_loop;
  FLARE_CHECK(!!ptr);
  return ptr.get();
}

void AllEventLoopsBarrier() {
  fiber::Latch l(event_loop_workers.size() *
                 FLAGS_flare_event_loop_per_scheduling_group);
  for (auto&& elws : event_loop_workers) {
    for (auto&& elw : elws) {
      elw.event_loop->AddTask([&] { l.count_down(); });
    }
  }
  l.wait();
}

void StopAllEventLoops() {
  if (FLAGS_flare_enable_watchdog) {
    watchdog.Stop();
  }
  for (auto&& elws : event_loop_workers) {
    for (auto&& elw : elws) {
      elw.event_loop->Stop();
    }
  }
}

void JoinAllEventLoops() {
  if (FLAGS_flare_enable_watchdog) {
    watchdog.Join();
  }
  for (auto&& elws : event_loop_workers) {
    for (auto&& elw : elws) {
      elw.event_loop->Join();
    }
  }
  for (auto&& elws : event_loop_workers) {
    for (auto&& elw : elws) {
      elw.fiber.join();
    }
  }
  // The event loop (internally) uses object pool, which requires all objects to
  // be returned before leaving `main`. (Otherwise a leak is possible.)
  //
  // Explicitly destroying `EventLoop`s achieves this.
  event_loop_workers.clear();
}

}  // namespace flare
