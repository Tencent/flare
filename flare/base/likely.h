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

#ifndef FLARE_BASE_LIKELY_H_
#define FLARE_BASE_LIKELY_H_

#if __GNUC__
#define FLARE_LIKELY(expr) (__builtin_expect(!!(expr), 1))
#define FLARE_UNLIKELY(expr) (__builtin_expect(!!(expr), 0))
#else
#define FLARE_LIKELY(expr) (expr)
#define FLARE_UNLIKELY(expr) (expr)
#endif

#endif  // FLARE_BASE_LIKELY_H_
