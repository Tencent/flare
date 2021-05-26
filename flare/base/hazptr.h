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

#ifndef FLARE_BASE_HAZPTR_H_
#define FLARE_BASE_HAZPTR_H_

// Implementation of hazard pointer.
//
// Inspired by:
// https://github.com/facebook/folly/blob/master/folly/synchronization/Hazptr.h
//
// @sa: https://en.wikipedia.org/wiki/Hazard_pointer
// @sa: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0566r4.pdf
// @sa: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1121r0.pdf

// Hazard pointer is useful in mostly-read cases. It helps you to keep a pointer
// (or, to be accurate, the object it points to) alive, with relatively low
// read-side overhead. For write-side, you must use your own synchronization
// mechanisms to serialize mutation.
//
// As an example, to do double-buffering (A design that use a buffer for
// reading, and another buffer for updating, which eventually replaces the
// reading buffer, and thereafter readers start reading the new buffer.):
//
// ```cpp
// struct Buffer : public HazptrObject<Buffer> {
//   // ... data
// };
//
// std::atomic<Buffer*> shared_buffer;
//
// void ReaderSide() {
//   Hazptr hazptr;
//   auto p = hazptr.Keep(&shared_buffer);
//
//   // Work on `p`. `p` is guaranteed to be alive whether or not `WriteSide()`
//   // is alive at the same time.
//   ...
//
//   // Once `hazptr` is destroyed, `p` is eligible for reclamation (if it's
//   // already been `Retire()`-d.)
// }
//
// void WriterSide() {
//   // Although not required by hazard pointer itself, you normally should use
//   // some synchronization mechanism (e.g., `std::mutex`) to avoid creating
//   // more than 1 "updating" buffer at the same time.
//   auto new_buffer = std::make_unique<Buffer>();
//   // ... Fill `new_buffer`.
//   auto old = shared_buffer.exchange(new_buffer.release(),
//       // ORDER AT LEAST AS STRONG AS `memory_order_acq_rel` IS REQUIRED.
//       std::memory_order_acq_rel);  // Update buffer pointer.
//   old->Retire();   // `old` will be released after (but not necessary
//                    // immediately upon) no `ReaderSide` referencing it.
// }
// ```

#include "flare/base/hazptr/hazptr.h"
#include "flare/base/hazptr/hazptr_domain.h"
#include "flare/base/hazptr/hazptr_object.h"

#endif  // FLARE_BASE_HAZPTR_H_
