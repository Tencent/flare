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

#include "flare/base/status.h"

#include "flare/base/internal/early_init.h"
#include "flare/base/internal/logging.h"

namespace flare {

Status::Status(int status, const std::string& desc) {
  if (status == 0) {
    FLARE_LOG_ERROR_IF_ONCE(
        !desc.empty(),
        "Status `SUCCESS` may not carry description, but [{}] is given.", desc);
    // NOTHING else.
  } else {
    state_ = MakeRefCounted<State>();
    state_->status = status;
    state_->desc = desc;
  }
}

const std::string& Status::message() const noexcept {
  return !state_ ? internal::EarlyInitConstant<std::string>() : state_->desc;
}

std::string Status::ToString() const {
  return fmt::format(
      "[{}] {}", code(),
      !ok() ? message() : "The operation completed successfully.");
}

}  // namespace flare
