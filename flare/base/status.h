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

#ifndef FLARE_BASE_STATUS_H_
#define FLARE_BASE_STATUS_H_

#include <string>
#include <type_traits>

#include "flare/base/internal/early_init.h"
#include "flare/base/internal/meta.h"
#include "flare/base/ref_ptr.h"

namespace flare {

namespace detail {}  // namespace detail

// This class describes status code, as its name implies.
//
// `0` is treated as success, other values are failures.
class Status {
 public:
  Status() noexcept = default;
  explicit Status(int status, const std::string& desc =
                                  internal::EarlyInitConstant<std::string>());

  // If an enum type is inherited from `int` (which is the default), we allow
  // constructing `Status` from that type without further casting.
  //
  // Special note: `Status` treats 0 as success. If enumerator with value 0 in
  // `T` is not a successful status, you need to take special care when using
  // it.
  template <class T, class = std::enable_if_t<
                         std::is_same_v<internal::underlying_type_t<T>, int>>>
  explicit Status(T status, const std::string& desc =
                                internal::EarlyInitConstant<std::string>())
      : Status(static_cast<int>(status), desc) {}

  // Test if this object represents a successful status.
  bool ok() const noexcept { return !state_; }

  // Get status value.
  int code() const noexcept { return !state_ ? 0 : state_->status; }

  // Get description of the status.
  const std::string& message() const noexcept;

  // Returns a human readable string describing the status.
  std::string ToString() const;

 private:
  struct State : RefCounted<State> {
    int status;
    std::string desc;
  };
  // For successful state, we use `nullptr` here. (For performance reasons.).
  RefPtr<State> state_;
};

}  // namespace flare

#endif  // FLARE_BASE_STATUS_H_
