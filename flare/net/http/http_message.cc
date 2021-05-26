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

#include "flare/net/http/http_message.h"

#include "flare/base/enum.h"
#include "flare/base/internal/early_init.h"
#include "flare/base/logging.h"

namespace flare::http {

std::string* HttpMessage::StringifyBody() {
  if (!body_) {
    body_str_ = "";
  } else {
    body_str_ = FlattenSlow(*body_);
  }
  return &*body_str_;
}

}  // namespace flare::http
