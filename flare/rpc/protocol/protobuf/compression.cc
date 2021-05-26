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

#include "flare/rpc/protocol/protobuf/compression.h"

#include <string>
#include <vector>

#include "flare/base/compression.h"

namespace flare::protobuf::compression {
namespace {

const std::vector<std::string> kCompressionName = {
    "",           // COMPRESSION_ALGORITHM_UNKNOWN
    "",           // COMPRESSION_ALGORITHM_NONE
    "gzip",       // COMPRESSION_ALGORITHM_GZIP
    "lz4-frame",  // COMPRESSION_ALGORITHM_LZ4_FRAME
    "snappy",     // COMPRESSION_ALGORITHM_SNAPPY
    "zstd",       // COMPRESSION_ALGORITHM_ZSTD
};

}  // namespace

bool DecompressBodyIfNeeded(const rpc::RpcMeta& meta, NoncontiguousBuffer body,
                            NoncontiguousBuffer* buffer) {
  // Specialized for zstd.
  thread_local auto prioritized_decompressor = MakeDecompressor("zstd");
  if (meta.has_compression_algorithm()) {
    auto compression = meta.compression_algorithm();
    if (compression != rpc::COMPRESSION_ALGORITHM_NONE) {
      bool is_prioritized_compression =
          compression == rpc::COMPRESSION_ALGORITHM_ZSTD;
      std::string name;
      if (compression >= kCompressionName.size()) {
        FLARE_LOG_WARNING_EVERY_SECOND("Unknown compression {}", compression);
        return false;
      }
      if (auto decompressed = Decompress(
              is_prioritized_compression
                  ? prioritized_decompressor.get()
                  : MakeDecompressor(kCompressionName[compression]).get(),
              body);
          decompressed) {
        *buffer = std::move(*decompressed);
        return true;
      } else {
        FLARE_LOG_WARNING_EVERY_SECOND("Compression failed {}", compression);
        if (is_prioritized_compression) {
          prioritized_decompressor = MakeDecompressor("zstd");
        }
        return false;
      }
    }
  }
  *buffer = std::move(body);
  return true;
}

std::size_t CompressBodyIfNeeded(const rpc::RpcMeta& meta,
                                 const ProtoMessage& msg,
                                 NoncontiguousBufferBuilder* builder) {
  if (meta.has_compression_algorithm() &&
      meta.compression_algorithm() != rpc::COMPRESSION_ALGORITHM_NONE) {
    NoncontiguousBufferBuilder nbb;
    if (WriteTo(msg.msg_or_buffer, &nbb)) {
      return CompressBufferIfNeeded(meta, nbb.DestructiveGet(), builder);
    } else {
      return 0;
    }
  }
  return WriteTo(msg.msg_or_buffer, builder);
}

std::size_t CompressBufferIfNeeded(const rpc::RpcMeta& meta,
                                   const NoncontiguousBuffer& buffer,
                                   NoncontiguousBufferBuilder* builder) {
  // Specialized for zstd.
  thread_local auto prioritized_compressor = MakeCompressor("zstd");
  if (meta.has_compression_algorithm()) {
    auto compression = meta.compression_algorithm();
    if (compression != rpc::COMPRESSION_ALGORITHM_NONE) {
      bool is_prioritized_compression =
          compression == rpc::COMPRESSION_ALGORITHM_ZSTD;
      auto old_size = builder->ByteSize();
      std::string name;
      FLARE_CHECK(compression < kCompressionName.size(),
                  "Unknown Compression {}", compression);
      FLARE_CHECK(
          Compress(is_prioritized_compression
                       ? prioritized_compressor.get()
                       : MakeCompressor(kCompressionName[compression]).get(),
                   buffer, builder),
          "Compression failed {}", compression);
      return builder->ByteSize() - old_size;
    }
  }
  builder->Append(buffer);
  return buffer.ByteSize();
}

}  // namespace flare::protobuf::compression
