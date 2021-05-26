// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_BASE_INTERNAL_DPC_H_
#define FLARE_BASE_INTERNAL_DPC_H_

#include "flare/base/function.h"

namespace flare::internal {

// Deferred procedure calls.
//
// For better responsiveness (in critical path), you can schedule jobs which
// needs not to be done immediately to be executed later by queuing a DPC.
//
// Insipred by DPC of Windows NT Kernel.
//
// Scheduling DPC is relative fast, but keep in mind that DPC's execution can be
// deferred arbitrarily.
//
// Internally DPC queues is flushed periodically, and then, run by
// `BackgroundTaskHost`, so be careful not to overload the background workers.
void QueueDpc(Function<void()>&& cb) noexcept;

// Flush all pending DPCs to background task host.
void FlushDpcs() noexcept;

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_DPC_H_
