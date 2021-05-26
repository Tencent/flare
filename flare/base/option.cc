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

#include "flare/base/option.h"

#include <utility>

#include "flare/base/option/option_service.h"

namespace flare {

namespace option {

void InitializeOptions() { option::OptionService::Instance()->ResolveAll(); }

void ShutdownOptions() { option::OptionService::Instance()->Shutdown(); }

void SynchronizeOptions() {
  option::OptionService::Instance()->UpdateOptions();
}

Json::Value DumpOptions() { return option::OptionService::Instance()->Dump(); }

}  // namespace option

}  // namespace flare
