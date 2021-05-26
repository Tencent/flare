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

#ifndef FLARE_BASE_OBJECT_POOL_GLOBAL_H_
#define FLARE_BASE_OBJECT_POOL_GLOBAL_H_

#include "flare/base/object_pool/types.h"

// Extra requirement on `PoolTraits<T>`:
//
// - Not implemented yet.

// `PoolType::Global`
namespace flare::object_pool::detail::global {

void* Get(const TypeDescriptor& desc);
void Put(const TypeDescriptor& desc, void* ptr);

}  // namespace flare::object_pool::detail::global

#endif  // FLARE_BASE_OBJECT_POOL_GLOBAL_H_
