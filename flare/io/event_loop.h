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

#ifndef FLARE_IO_EVENT_LOOP_H_
#define FLARE_IO_EVENT_LOOP_H_

#include <sys/epoll.h>

#include <atomic>
#include <chrono>
#include <list>
#include <mutex>

#include "flare/base/function.h"
#include "flare/base/handle.h"
#include "flare/io/descriptor.h"
#include "flare/io/detail/event_loop_notifier.h"

struct epoll_event;

namespace flare {

// Instantiating this class is an error. Use `GetGlobalEventLoop` instead.
class EventLoop {
 public:
  // Not for public use. Call `util::GetGlobalEventLoop` instead.
  EventLoop();
  ~EventLoop();

  // The `desc`'s callback may be called even before this method returns. If
  // this is inconvenient to you, specify `enabled` as `false`, and
  // `EnableDescriptor` it when you see appropriate.
  //
  // `EventLoop` may add extra flags as it sees fit via `desc->SetEventMask()`.
  void AttachDescriptor(Descriptor* desc, bool enabled = true);

  // If the descriptor was attached with `enabled` not set or has been
  // previously disabled via `DisableDescriptor`, you can call this method to
  // enable it (but only for the first time, after which you should call
  // `RearmDescriptor` as usual.).
  void EnableDescriptor(Descriptor* desc);

  // You'll likely want to call this method in EventLoop's thread (via
  // `AddTask`), for it may race will `Suppress` returned by `Descriptor`'s
  // callbacks if called in different threads.
  //
  // The new event mask is get from `desc->GetEventMask`. You need to update it
  // beforehand.
  void RearmDescriptor(Descriptor* desc);

  // Suppress all event associated with `desc` from happening.
  void DisableDescriptor(Descriptor* desc);

  // The event loop is guaranteed not to touch `desc` if at least one task
  // posted after `DetachDescriptor` is executed. (@sa: `Barrier()`)
  //
  // Note that even if after `DetachDescriptor()` returns, `desc`'s callback can
  // still be called. This ONLY guarantee is that after a task has been
  // executed, the event loop won't touch `desc.
  //
  // This convention is weird, but due to its concurrent nature, unless we
  // block the caller, we can't guarantee we won't race with the event loop
  // thread (we may even be using the `Descriptor` when called.).
  //
  // It guaranteed any tasks posted before calling `DetachDescriptor()` is done
  // prior the descriptor is *actually* detached.
  void DetachDescriptor(Descriptor* desc);

  // Tasks are run after all the events on `Descriptor`'s are processed.
  //
  // The event loop guarantees that all the tasks are executed before
  // fully stopped.
  //
  // CAUTION: Only call this in inevitable cases. This method is not meant to be
  // used as a general way for running jobs in the background / asynchronously.
  // It's either intended to be called in a frequent fashion. If either is what
  // you want, you likely should be using `Async` instead.
  void AddTask(Function<void()>&& cb);

  // Post a task and wait for it to return.
  void Barrier();

  // Won't return until `Stop()` is called.
  void Run();

  void Stop();
  void Join();

  // I don't think there's something to "join" as we do not own an execution
  // context.
  //
  // Join the fiber running the event loop instead.
  //
  // void Join();

  // Return the event loop we're running inside, or nullptr if we're not running
  // in any event loop.
  static EventLoop* Current();

 private:
  void WaitAndRunEvents(std::chrono::milliseconds wait_for);
  void RunUserTasks();
  void RunEventHandlers(epoll_event* begin, epoll_event* end);

 private:
  std::atomic<bool> exiting_{false};
  Handle epfd_;

  // `notifier_` is used for waking the worker. (e.g. in the case there's a new
  // task for running.)
  io::detail::EventLoopNotifier notifier_;

  std::mutex tasks_lock_;
  // Do NOT use `std::deque` here. Constructing empty `std::deque`
  // (`RunUserTasks()` does this each time it get called to move tasks here out)
  // incurs memory allocation. `std::list` won't. OTOH, We don't expect too many
  // task here, so excessive node allocation done by `std::list` is not a
  // problem.
  std::list<Function<void()>> tasks_;
};

// There is, in fact, one event loop for worker group (@sa `WorkerPool`).
//
// We might provide a flag to specify (a hint of?) number of event loops in
// each NUMA domain.
void StartAllEventLoops();

// `scheduling_group` is used for selecting the scheduling group, `fd` is then
// used for selecting event loop inside the node.
//
// Passing `-1` or `0` to `GetGlobalEventLoop` is an error, as they're invalid
// fd values.
EventLoop* GetGlobalEventLoop(std::size_t scheduling_group, int fd = -2);

// Wait until each event loop had executed user's task at least once.
//
// Primarily used in shutdown process.
void AllEventLoopsBarrier();

void StopAllEventLoops();
void JoinAllEventLoops();

}  // namespace flare

#endif  // FLARE_IO_EVENT_LOOP_H_
