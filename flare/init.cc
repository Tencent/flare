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

#include "flare/init.h"

#include <sys/signal.h>

#include "thirdparty/gflags/gflags.h"
#include "thirdparty/glog/logging.h"
#include "thirdparty/glog/raw_logging.h"

#include "flare/base/buffer.h"
#include "flare/base/internal/background_task_host.h"
#include "flare/base/internal/dpc.h"
#include "flare/base/internal/time_keeper.h"
#include "flare/base/logging.h"
#include "flare/base/monitoring/init.h"
#include "flare/base/option.h"
#include "flare/base/random.h"
#include "flare/base/thread/latch.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/runtime.h"
#include "flare/fiber/this_fiber.h"
#include "flare/init/on_init.h"
#include "flare/init/override_flag.h"
#include "flare/io/event_loop.h"
#include "flare/net/http/http_client.h"
#include "flare/net/internal/http_engine.h"
#include "flare/rpc/binlog/init.h"
#include "flare/rpc/internal/stream_call_gate_pool.h"

using namespace std::literals;

DEFINE_bool(flare_abort_on_double_quit_signal, true,
            "If set, flare aborts the whole program when a second SIGQUIT (or "
            "SIGINT) is received. This helps when the program cannot exit "
            "cleanly on the first signal.");

namespace flare {

namespace {

std::atomic<bool> g_quit_signal = false;

void QuitSignalHandler(int sig) {
  auto old = g_quit_signal.exchange(true, std::memory_order_relaxed);
  if (old && FLAGS_flare_abort_on_double_quit_signal) {
    RAW_LOG(FATAL, "Double quit signal received. Crashing the program.");
  }
}

void InstallQuitSignalHandler() {
  static std::once_flag f;

  std::call_once(f, [&] {
    FLARE_PCHECK(signal(SIGINT, QuitSignalHandler) != SIG_ERR);   // [Ctrl + C]
    FLARE_PCHECK(signal(SIGQUIT, QuitSignalHandler) != SIG_ERR);  // [Ctrl + \]
    FLARE_PCHECK(signal(SIGTERM, QuitSignalHandler) != SIG_ERR);
  });
}

// Prewarm some frequently used object pools.
void PrewarmObjectPools() {
  constexpr auto kFibersPerSchedulingGroup = 1024;
  constexpr auto kBufferSizePerFiber = 131072;

  for (std::size_t i = 0; i != fiber::GetSchedulingGroupCount(); ++i) {
    for (int j = 0; j != kFibersPerSchedulingGroup; ++j) {
      // Pre-allocate some fiber stacks for use.
      fiber::internal::StartFiberDetached(
          Fiber::Attributes{.scheduling_group = i}, [] {
            // Warms object pool used by `NoncontiguousBuffer`.
            static char temp[kBufferSizePerFiber];
            [[maybe_unused]] NoncontiguousBufferBuilder builder;
            builder.Append(temp, sizeof(temp));
          });
    }
  }
}

}  // namespace

// Crash on failure.
int Start(int argc, char** argv, Function<int(int, char**)> cb) {
  google::InstallFailureSignalHandler();

  google::ParseCommandLineFlags(&argc, &argv, true);
  detail::ApplyFlagOverrider();

  google::InitGoogleLogging(argv[0]);

  // No you can't install a customized `Future` executor to run `Future`'s
  // continuations in new fibers.
  //
  // The default executor change has a global (not only in flare's context)
  // effect, and will likely break program if `Future`s are also used in pthread
  // context.
  //
  // DO NO INSTALL A CUSTOMIZED EXECUTOR TO RUN `FUTURE` IN NEW FIBER.

  // This is a bit late, but we cannot write log (into file) before glog is
  // initialized.
  FLARE_LOG_INFO("Flare started.");

  FLARE_PCHECK(signal(SIGPIPE, SIG_IGN) != SIG_ERR);

  InitializeBasicRuntime();
  detail::RunAllInitializers();
  fiber::StartRuntime();

  FLARE_LOG_INFO("Flare runtime initialized.");

  // Now we start to run in fiber environment.
  int rc = 0;
  {
    Latch l(1);
    fiber::internal::StartFiberDetached([&] {
      StartAllEventLoops();
      PrewarmObjectPools();  // To minimize slowness on startup.

      object_pool::detail::memory_node_shared::StartPeriodicalCacheWasher();
      option::InitializeOptions();  // It this too late?
      monitoring::InitializeMonitoringSystem();
      binlog::InitializeBinlog();

      rc = cb(argc, argv);  // User's callback.

      rpc::internal::StopAllGlobalStreamCallGatePools();
      rpc::internal::JoinAllGlobalStreamCallGatePools();

      internal::HttpEngine::Stop();
      internal::HttpEngine::Join();

      StopAllEventLoops();
      JoinAllEventLoops();

      monitoring::TerminateMonitoringSystem();
      option::ShutdownOptions();
      object_pool::detail::memory_node_shared::StopPeriodicalCacheWasher();

      l.count_down();
    });  // Don't `join()` here, we can't use fiber synchronization
         // primitives outside of fiber context.
    l.wait();
  }

  fiber::TerminateRuntime();
  detail::RunAllFinalizers();
  TerminateBasicRuntime();

  FLARE_LOG_INFO("Exited");
  return rc;
}

void WaitForQuitSignal() {
  // We only capture quit signal(s) if we're called. This allows users to handle
  // these signals themselves (by not calling this method) if they want.
  InstallQuitSignalHandler();

  while (!g_quit_signal.load(std::memory_order_relaxed)) {
    this_fiber::SleepFor(100ms);
  }
  FLARE_LOG_INFO("Quit signal received.");
}

bool CheckForQuitSignal() {
  InstallQuitSignalHandler();
  return g_quit_signal.load(std::memory_order_relaxed);
}

void InitializeBasicRuntime() {
  internal::BackgroundTaskHost::Instance()->Start();
  internal::TimeKeeper::Instance()->Start();
}

void TerminateBasicRuntime() {
  internal::FlushDpcs();
  internal::TimeKeeper::Instance()->Stop();
  internal::TimeKeeper::Instance()->Join();
  internal::BackgroundTaskHost::Instance()->Stop();
  internal::BackgroundTaskHost::Instance()->Join();
}

}  // namespace flare
