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

#ifndef FLARE_BASE_THREAD_OUT_OF_DUTY_CALLBACK_H_
#define FLARE_BASE_THREAD_OUT_OF_DUTY_CALLBACK_H_

#include <chrono>
#include <cinttypes>

#include "flare/base/function.h"

namespace flare {

// "Out of duty" is defined as the time period when the caller is not currently
// working on something important (e.g., not in critical path of handling RPC.).
//
// In such period of time, the thread may well do some book-keeping stuff such
// as flushing thread-locally cached monitoring reports, etc.
//
// Here we provide a mechanism for accomplishing this. However, as obvious, this
// requires cooperation from "actual" thread workers.
//
// For the moment our fiber runtime would notify us about out-of-duty events as
// appropriate. Threads outside should notify Flare themselves.
//
// Note that, however, components should NOT rely on their callbacks to be
// called periodically. This mechanism is ONLY meant to be used as a way to
// flush thread-local cache in a *more* timely fashion, but not a catch-all
// solution. If some thread is busy all the time, or (in case it's a foreign
// thread to Flare) the thread does not notify us about its being free at all,
// you should still implement your own "check-delay-and-report" logic to make
// sure things work as intended.
//
// Methods in this file only deals with "thread-level" out-of-duty events, if
// you want to do some "global" book-keeping stuff, just spawn a new (low
// priority) thread or queue a DPC instead.
//
// Caution about memory leak: For the moment our implementation does NOT work
// well with frequent thread-creation and destruction. In such case memory WILL
// leak.
//
// Performance note: Setting / deleteing out-of-duty callback is a heavy-lifting
// operation. Therefore, it's suggested to avoid calling these two methods
// frequently. If you just want a "one-shot" book-keeping operation to be
// performed, resort to DPC.

// Set a callback to be called whenver `NotifyThreadOutOfDutyCallbacks()` is
// called (regardless of thread context).
//
// `callback` should be thread-safe.
//
// To avoid excessive calls to `callback`, for a given thread, a second call to
// `callback` won't happen unless `min_interval` has elapsed since the last
// call. (Note that, however, it's possible you're called less often, or not be
// called at all.)
std::uint64_t SetThreadOutOfDutyCallback(Function<void()> callback,
                                         std::chrono::nanoseconds min_interval);

// Delete a previously-set callback.
void DeleteThreadOutOfDutyCallback(std::uint64_t handle);

// Notifies Flare that the calling thread does not have other important things
// to do.
void NotifyThreadOutOfDutyCallbacks();

}  // namespace flare

#endif  // FLARE_BASE_THREAD_OUT_OF_DUTY_CALLBACK_H_
