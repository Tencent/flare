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

#ifndef FLARE_BASE_MONITORING_INIT_H_
#define FLARE_BASE_MONITORING_INIT_H_

#include <string>

#include "flare/base/function.h"

namespace flare::monitoring {

// Initialize monitoring system. This is called by `flare::Start()` (as the
// initialization cannot be done before finishing parsing GFlags) and may not be
// called by users.
void InitializeMonitoringSystem();

// Terminate monitoring system. Called by `flare::Start()`, you shouldn't call
// it manually.
void TerminateMonitoringSystem();

// Used by builtin monitoring utilities. This method registers a callback once
// monitoring systems are initialized. The callback is called with an empty
// string if the registered key is not enabled by user, or the remapped key if
// it's set.
//
// If the monitoring system has already been initialized by the time this method
// is called, `cb` is called immediately.
void RegisterBuiltinMonitoringKeyCallback(
    const std::string& key, Function<void(const std::string&)> cb);

}  // namespace flare::monitoring

#endif  // FLARE_BASE_MONITORING_INIT_H_
