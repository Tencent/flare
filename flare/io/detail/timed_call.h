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

#ifndef FLARE_IO_DETAIL_TIMED_CALL_H_
#define FLARE_IO_DETAIL_TIMED_CALL_H_

#include <utility>

#include "thirdparty/gflags/gflags_declare.h"

#include "flare/base/deferred.h"
#include "flare/base/logging.h"
#include "flare/base/tsc.h"

DECLARE_bool(flare_io_dump_slow_calls);

namespace flare::io::detail {

// Write a warning log if running `f` cost more than `tolerance` time.
//
// `name` should not be capitalized.
//
// e.g.: "Running name cost 100 millisecond."
template <class F, class C, class... S>
inline auto TimedCall(F&& f, C&& tolerance, S&&... s) {
  static_assert(std::is_void_v<std::invoke_result_t<F>>,
                "Not implemented: TimedCall for functors retuning non-void.");
  if (!FLAGS_flare_io_dump_slow_calls) {
    return std::forward<F>(f)();
  } else {
    ScopedDeferred sd([&, start = ReadTsc()] {
      if (auto used = DurationFromTsc(start, ReadTsc());
          FLARE_UNLIKELY(used > std::forward<C>(tolerance))) {
        [&]() __attribute((noinline)) {
          FLARE_LOG_WARNING_EVERY_SECOND("{} costs {} millisecond(s).",
                                         fmt::format(std::forward<S>(s)...),
                                         used / std::chrono::milliseconds(1));
        }
        ();
      }
    });
    return std::forward<F>(f)();
  }
}

}  // namespace flare::io::detail

#endif  // FLARE_IO_DETAIL_TIMED_CALL_H_
