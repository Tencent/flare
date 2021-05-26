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

#include "flare/fiber/detail/fiber_worker.h"

#include <thread>

#include "flare/base/logging.h"
#include "flare/base/random.h"
#include "flare/base/thread/attribute.h"
#include "flare/base/thread/out_of_duty_callback.h"
#include "flare/fiber/detail/fiber_entity.h"
#include "flare/fiber/detail/scheduling_group.h"

namespace flare::fiber::detail {

FiberWorker::FiberWorker(SchedulingGroup* sg, std::size_t worker_index)
    : sg_(sg), worker_index_(worker_index) {}

void FiberWorker::AddForeignSchedulingGroup(SchedulingGroup* sg,
                                            std::uint64_t steal_every_n) {
  victims_.push({.sg = sg,
                 .steal_every_n = steal_every_n,
                 .next_steal = Random(steal_every_n)});
}

void FiberWorker::Start(bool no_cpu_migration) {
  FLARE_CHECK(!no_cpu_migration || !sg_->Affinity().empty());
  worker_ = std::thread([this, no_cpu_migration] {
    if (!sg_->Affinity().empty()) {
      if (no_cpu_migration) {
        FLARE_CHECK_LT(worker_index_, sg_->Affinity().size());
        auto cpu = sg_->Affinity()[worker_index_];

        SetCurrentThreadAffinity({cpu});
        FLARE_VLOG(10,
                   "Fiber worker #{} is started on dedicated processsor #{}.",
                   worker_index_, cpu);
      } else {
        SetCurrentThreadAffinity(sg_->Affinity());
      }
    }
    WorkerProc();
  });
}

void FiberWorker::Join() { worker_.join(); }

void FiberWorker::WorkerProc() {
  sg_->EnterGroup(worker_index_);

  while (true) {
    auto fiber = sg_->AcquireFiber();

    if (!fiber) {
      fiber = sg_->SpinningAcquireFiber();
      if (!fiber) {
        fiber = StealFiber();
        FLARE_CHECK_NE(fiber, SchedulingGroup::kSchedulingGroupShuttingDown);
        if (!fiber) {
          fiber = sg_->WaitForFiber();  // This one either sleeps, or succeeds.
          // `FLARE_CHECK_NE` does not handle `nullptr` well.
          FLARE_CHECK_NE(fiber, static_cast<FiberEntity*>(nullptr));
        }
      }
    }

    if (FLARE_UNLIKELY(fiber ==
                       SchedulingGroup::kSchedulingGroupShuttingDown)) {
      break;
    }

    fiber->Resume();

    // Notify the framework that any pending operations can be performed.
    NotifyThreadOutOfDutyCallbacks();
  }
  FLARE_CHECK_EQ(GetCurrentFiberEntity(), GetMasterFiberEntity());
  sg_->LeaveGroup();
}

FiberEntity* FiberWorker::StealFiber() {
  if (victims_.empty()) {
    return nullptr;
  }

  ++steal_vec_clock_;
  while (victims_.top().next_steal <= steal_vec_clock_) {
    auto&& top = victims_.top();
    if (auto rc = top.sg->RemoteAcquireFiber()) {
      // We don't pop the top in this case, since it's not empty, maybe the next
      // time we try to steal, there are still something for us.
      return rc;
    }
    victims_.push({.sg = top.sg,
                   .steal_every_n = top.steal_every_n,
                   .next_steal = top.next_steal + top.steal_every_n});
    victims_.pop();
    // Try next victim then.
  }
  return nullptr;
}

}  // namespace flare::fiber::detail
