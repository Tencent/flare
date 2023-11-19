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

#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <thread>
#include <utility>

#include "gtest/gtest.h"

#include "flare/base/random.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/latch.h"
#include "flare/fiber/this_fiber.h"
#include "flare/io/event_loop.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare {

std::atomic<std::size_t> cleaned{};

class PipeDesc : public Descriptor {
 public:
  explicit PipeDesc(Handle handle)
      : Descriptor(std::move(handle), Event::Write) {}
  EventAction OnReadable() override { return read_rc_; }
  EventAction OnWritable() override { return EventAction::Ready; }
  void OnError(int err) override {}
  void OnCleanup(CleanupReason reason) override { ++cleaned; }

  void SetReadAction(EventAction act) { read_rc_ = act; }

 private:
  EventAction read_rc_;
};

RefPtr<PipeDesc> CreatePipe() {
  int fds[2];
  PCHECK(pipe2(fds, O_NONBLOCK | O_CLOEXEC) == 0);
  PCHECK(write(fds[1], "asdf", 4) == 4);
  close(fds[1]);
  return MakeRefCounted<PipeDesc>(Handle(fds[0]));
}

TEST(Descriptor, ConcurrentRestartRead) {
  for (auto&& action :
       {Descriptor::EventAction::Ready, Descriptor::EventAction::Suppress}) {
    for (int i = 0; i != 10000; ++i) {
      Fiber fibers[2];
      fiber::Latch latch(1);
      auto desc = CreatePipe();
      auto ev = GetGlobalEventLoop(0, desc->fd());

      desc->SetReadAction(action);
      ev->AttachDescriptor(desc.Get(), true);

      fibers[0] = Fiber([desc, &latch] {
        latch.wait();
        desc->RestartReadIn(0ns);
      });
      fibers[1] = Fiber([desc, &latch] {
        latch.wait();
        desc->Kill(Descriptor::CleanupReason::Closing);
      });
      latch.count_down();
      for (auto&& e : fibers) {
        e.join();
      }
    }
  }
  while (cleaned != 10000 * 2) {
    this_fiber::SleepFor(1ms);
  }
}

}  // namespace flare

FLARE_TEST_MAIN
