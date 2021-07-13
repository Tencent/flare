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

#include "flare/base/compression.h"

#include <memory>
#include <optional>
#include <string_view>

#include "flare/base/buffer/compression_output_stream.h"

namespace flare {

std::unique_ptr<Compressor> MakeCompressor(std::string_view name) {
  return compressor_registry.TryNew(name);
}

std::unique_ptr<Decompressor> MakeDecompressor(std::string_view name) {
  return decompressor_registry.TryNew(name);
}

std::optional<NoncontiguousBuffer> Decompress(Decompressor* decompressor,
                                              const NoncontiguousBuffer& nb) {
  NoncontiguousBufferBuilder builder;
  if (!Decompress(decompressor, nb, &builder)) {
    return std::nullopt;
  }
  return builder.DestructiveGet();
}

std::optional<NoncontiguousBuffer> Decompress(Decompressor* decompressor,
                                              std::string_view body) {
  NoncontiguousBufferBuilder builder;
  if (!Decompress(decompressor, body, &builder)) {
    return std::nullopt;
  }
  return builder.DestructiveGet();
}

bool Decompress(Decompressor* decompressor, const NoncontiguousBuffer& nb,
                NoncontiguousBufferBuilder* builder) {
  if (!decompressor) {
    FLARE_LOG_WARNING_EVERY_SECOND("Compressor nullptr");
    return false;
  }
  NoncontiguousBufferCompressionOutputStream out(builder);
  return decompressor->Decompress(nb, &out);
}

bool Decompress(Decompressor* decompressor, std::string_view body,
                NoncontiguousBufferBuilder* builder) {
  if (!decompressor) {
    FLARE_LOG_WARNING_EVERY_SECOND("Compressor nullptr");
    return false;
  }
  NoncontiguousBufferCompressionOutputStream out(builder);
  return decompressor->Decompress(body.data(), body.size(), &out);
}

std::optional<NoncontiguousBuffer> Compress(Compressor* compressor,
                                            const NoncontiguousBuffer& nb) {
  NoncontiguousBufferBuilder builder;
  if (!Compress(compressor, nb, &builder)) {
    return std::nullopt;
  }
  return builder.DestructiveGet();
}

bool Compress(Compressor* compressor, const NoncontiguousBuffer& nb,
              NoncontiguousBufferBuilder* builder) {
  if (!compressor) {
    FLARE_LOG_WARNING_EVERY_SECOND("Compressor nullptr");
    return false;
  }
  NoncontiguousBufferCompressionOutputStream out(builder);
  return compressor->Compress(nb, &out);
}

std::optional<NoncontiguousBuffer> Compress(Compressor* compressor,
                                            std::string_view body) {
  NoncontiguousBufferBuilder builder;
  if (!Compress(compressor, body, &builder)) {
    return std::nullopt;
  }
  return builder.DestructiveGet();
}

bool Compress(Compressor* compressor, std::string_view body,
              NoncontiguousBufferBuilder* builder) {
  if (!compressor) {
    FLARE_LOG_WARNING_EVERY_SECOND("Compressor nullptr");
    return false;
  }
  NoncontiguousBufferCompressionOutputStream out(builder);
  return compressor->Compress(body.data(), body.size(), &out);
}

}  // namespace flare
