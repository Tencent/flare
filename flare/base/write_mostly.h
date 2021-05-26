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

#ifndef FLARE_BASE_WRITE_MOSTLY_H_
#define FLARE_BASE_WRITE_MOSTLY_H_

// For perf. sensitive scenarios, we provides some optimized-for-writer classes
// to do basic statistics.
//
// See headers below for more details.
#include "flare/base/write_mostly/basic_ops.h"
#include "flare/base/write_mostly/metrics.h"

// In case you want to specialize for your own type, we also provide this basic
// template.
#include "flare/base/write_mostly/write_mostly.h"

#endif  // FLARE_BASE_WRITE_MOSTLY_H_
