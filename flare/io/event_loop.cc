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

#include <fcntl.h>
#include <sys/epoll.h>

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

constexpr auto kEpollError = EPOLLERR;
constexpr auto kExtraEpollFlags = EPOLLET;  // `EPOLLONESHOT`?

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
  // @sa: https://linux.die.net/man/2/epoll_create1
  //
  // > Since Linux 2.6.8, the size argument is ignored, but must be greater than
  // > zero; see NOTES below.
  //
  // > epoll_create1() was added to the kernel in version 2.6.27. Library
  // > support is provided in glibc starting with version 2.9.
  //
  // We use `epoll_create` here since `epoll_create1` is not available on
  // CentOS 6. `epoll_create` does not support `EPOLL_CLOEXEC` though.
  epfd_.Reset(epoll_create(1));
  FLARE_PCHECK(epfd_.Get() != -1);
  auto oldflags = fcntl(epfd_.Get(), F_GETFD);
  FLARE_PCHECK(oldflags != -1);
  FLARE_PCHECK(fcntl(epfd_.Get(), F_SETFD, oldflags | FD_CLOEXEC) == 0);

  // `EventLoopNotifier` is different in that its `OnReadable` must be called
  // synchronously (to avoid wake-up loss), and hence must be handled
  // individually.
  epoll_event ee;
  ee.events = EPOLLIN | EPOLLERR;
  ee.data.ptr = static_cast<void*>(&notifier_);
  FLARE_CHECK(epoll_ctl(epfd_.Get(), EPOLL_CTL_ADD, notifier_.fd(), &ee) == 0,
              "Failed to add notifier to event loop.");
}

EventLoop::~EventLoop() {
  // Does not makes much sense as both `notifier_.fd()` and `epfd_` it self is
  // going to be closed anyway.
  FLARE_PCHECK(
      epoll_ctl(epfd_.Get(), EPOLL_CTL_DEL, notifier_.fd(), nullptr) == 0,
      "Failed to remove notifier from event loop.");
}

void EventLoop::AttachDescriptor(Descriptor* desc, bool enabled) {
  desc->Ref();
  desc->SetEventMask(desc->GetEventMask() | kEpollError | kExtraEpollFlags);

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
  epoll_event ee;

  desc->SetEnabled(true);
  ee.events = desc->GetEventMask();
  ee.data.ptr = static_cast<void*>(desc);
  FLARE_PCHECK(epoll_ctl(epfd_.Get(), EPOLL_CTL_ADD, desc->fd(), &ee) == 0,
               "Failed to add fd #{} to epoll.", desc->fd());
  FLARE_VLOG(20, "Added descriptor [{}] with event mask [{}].", desc->GetName(),
             ee.events);
}

void EventLoop::RearmDescriptor(Descriptor* desc) {
  FLARE_CHECK(desc->Enabled(), "The descriptor is not enabled.");
  epoll_event ee;

  ee.events = desc->GetEventMask() | kEpollError | kExtraEpollFlags;
  ee.data.ptr = static_cast<void*>(desc);
  FLARE_VLOG(20, "Rearming descriptor [{}] with event mask [{}].",
             desc->GetName(), ee.events);
  FLARE_PCHECK(epoll_ctl(epfd_.Get(), EPOLL_CTL_MOD, desc->fd(), &ee) == 0,
               "Failed to modify fd #{} in epoll.", desc->fd());
}

void EventLoop::DisableDescriptor(Descriptor* desc) {
  FLARE_CHECK_EQ(desc->GetEventLoop(), this);
  FLARE_CHECK_EQ(EventLoop::Current(), this,
                 "This method must be called in event loop's context.");
  FLARE_CHECK(desc->Enabled(), "The descriptor is not enabled.");
  // http://man7.org/linux/man-pages/man2/epoll_ctl.2.html
  //
  // > In kernel versions before 2.6.9, the EPOLL_CTL_DEL operation required
  // > a non - null pointer in event, even though this argument is ignored.
  FLARE_PCHECK(epoll_ctl(epfd_.Get(), EPOLL_CTL_DEL, desc->fd(), nullptr) == 0,
               "Failed to remove fd #{} from epoll.", desc->fd());
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
  // FIXME: Need we use `epoll_pwait` instead to handle signal more
  // gracefully? (I'd say code using signal is fundamentally broken anyway.)
  constexpr auto kDescriptorsPerLoop = 128;
  epoll_event evs[kDescriptorsPerLoop];
  auto nfds = io::detail::EIntrSafeEpollWait(epfd_.Get(), evs, std::size(evs),
                                             wait_for / 1ms);
  FLARE_PCHECK(nfds >= 0, "Unexpected: epoll_wait failed.");

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

void EventLoop::RunEventHandlers(epoll_event* begin, epoll_event* end) {
  auto start_tsc = ReadTsc();
  ScopedDeferred _([&] {
    run_event_handlers_latency->Report(TscElapsed(start_tsc, ReadTsc()));
  });
  events_per_poll->Report(end - begin);

  static_assert(static_cast<int>(Descriptor::Event::Read) == EPOLLIN,
                "We're using `EPOLLIN` and `Descriptor::Event::Read` "
                "interchangably.");
  static_assert(static_cast<int>(Descriptor::Event::Write) == EPOLLOUT,
                "We're using `EPOLLOUT` and `Descriptor::Event::Write` "
                "interchangably.");

  while (begin != end) {
    // FIXME: This `if` is ugly.
    if (FLARE_UNLIKELY(begin->data.ptr == static_cast<void*>(&notifier_))) {
      FLARE_CHECK((begin->events & EPOLLERR) == 0,
                  "Unexpected error on event loop notifier.");
      notifier_.Reset();
      ++begin;
      continue;
    }

    auto desc = reinterpret_cast<Descriptor*>(begin->data.ptr);
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
