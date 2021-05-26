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

#include "flare/base/internal/logging.h"

#include <string>
#include <vector>

namespace flare::internal::logging {

namespace {

std::vector<PrefixAppender*>* GetProviders() {
  // Not using `NeverDestroyed` as we're "low-level" utility and therefore
  // should not bring in too many dependencies..
  static std::vector<PrefixAppender*> providers;
  return &providers;
}

}  // namespace

void InstallPrefixProvider(PrefixAppender* cb) {
  GetProviders()->push_back(cb);
}

void WritePrefixTo(std::string* to) {
  for (auto&& e : *GetProviders()) {
    auto was = to->size();
    e(to);
    if (to->size() != was) {
      to->push_back(' ');
    }
  }
}

}  // namespace flare::internal::logging
