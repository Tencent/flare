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

#ifndef FLARE_NET_REDIS_READER_H_
#define FLARE_NET_REDIS_READER_H_

#include <optional>

#include "flare/base/buffer.h"
#include "flare/net/redis/redis_object.h"

namespace flare::redis {

// Cut a Redis object from the buffer.
//
// Return positive on success, 0 if more data required, negative on error.
//
// On error, `buffer` is left in an INCONSISTENT state.
int TryCutRedisObject(NoncontiguousBuffer* buffer, RedisObject* object);

}  // namespace flare::redis

#endif  // FLARE_NET_REDIS_READER_H_
