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

#ifndef FLARE_BASE_BUFFER_PACKING_H_
#define FLARE_BASE_BUFFER_PACKING_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "flare/base/buffer.h"

namespace flare {

// Serializes [Key, Buffer]. Order in `kvs` is kept in the resulting buffer.
void WriteKeyedNoncontiguousBuffers(
    const std::vector<std::pair<std::string, NoncontiguousBuffer>>& kvs,
    NoncontiguousBufferBuilder* builder);
NoncontiguousBuffer WriteKeyedNoncontiguousBuffers(
    const std::vector<std::pair<std::string, NoncontiguousBuffer>>& kvs);

// Parses bytes produced by `WriteKeyedNoncontiguousBuffers`.
std::optional<std::vector<std::pair<std::string, NoncontiguousBuffer>>>
TryParseKeyedNoncontiguousBuffers(NoncontiguousBuffer buffer);

// Serializes a series of buffers. Order is kept (as obvious).
void WriteNoncontiguousBufferArray(
    const std::vector<NoncontiguousBuffer>& buffers,
    NoncontiguousBufferBuilder* builder);
NoncontiguousBuffer WriteNoncontiguousBufferArray(
    const std::vector<NoncontiguousBuffer>& buffers);

// Parses bytes produced by `WriteNoncontiguousBufferArray`.
std::optional<std::vector<NoncontiguousBuffer>>
TryParseNoncontiguousBufferArray(NoncontiguousBuffer buffer);

}  // namespace flare

#endif  // FLARE_BASE_BUFFER_PACKING_H_
