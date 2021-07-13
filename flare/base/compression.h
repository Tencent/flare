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

#ifndef FLARE_BASE_COMPRESSION_H_
#define FLARE_BASE_COMPRESSION_H_

#include <memory>
#include <optional>
#include <string_view>

#include "flare/base/compression/compression.h"

namespace flare {

// Name is the compression_algorithm and should already registered by
// FLARE_COMPRESSION_REGISTER_COMPRESSOR/FLARE_COMPRESSION_REGISTER_DECOMPRESSOR.
std::unique_ptr<Decompressor> MakeDecompressor(std::string_view name);

std::unique_ptr<Compressor> MakeCompressor(std::string_view name);

std::optional<NoncontiguousBuffer> Compress(Compressor* compressor,
                                            const NoncontiguousBuffer& nb);
std::optional<NoncontiguousBuffer> Compress(Compressor* compressor,
                                            std::string_view body);
bool Compress(Compressor* compressor, const NoncontiguousBuffer& nb,
              NoncontiguousBufferBuilder* builder);
bool Compress(Compressor* compressor, std::string_view body,
              NoncontiguousBufferBuilder* builder);

std::optional<NoncontiguousBuffer> Decompress(Decompressor* decompressor,
                                              const NoncontiguousBuffer& nb);
std::optional<NoncontiguousBuffer> Decompress(Decompressor* decompressor,
                                              std::string_view body);
bool Decompress(Decompressor* decompressor, const NoncontiguousBuffer& nb,
                NoncontiguousBufferBuilder* builder);
bool Decompress(Decompressor* decompressor, std::string_view body,
                NoncontiguousBufferBuilder* builder);

}  // namespace flare

#endif  // FLARE_BASE_COMPRESSION_H_
