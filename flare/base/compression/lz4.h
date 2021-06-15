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

#ifndef FLARE_BASE_COMPRESSION_LZ4_H_
#define FLARE_BASE_COMPRESSION_LZ4_H_

#include <functional>

#include "lz4/lz4frame.h"

#include "flare/base/compression/compression.h"

namespace flare::compression {

class Lz4FrameCompressor : public Compressor {
 public:
  Lz4FrameCompressor();
  ~Lz4FrameCompressor() override;
  bool Compress(const void* src, std::size_t size,
                CompressionOutputStream* out) override;
  bool Compress(const NoncontiguousBuffer& src,
                CompressionOutputStream* out) override;

 private:
  bool Append(const void* buffer, std::size_t size);
  bool Flush();
  bool Init(CompressionOutputStream* out);

  typedef std::function<std::size_t(void*, std::size_t)> DoCompressFunc;
  bool DoCompress(std::size_t size, DoCompressFunc f);
  CompressionOutputStream* out_ = nullptr;
  LZ4F_compressionContext_t ctx_;
};

class Lz4FrameDecompressor : public Decompressor {
 public:
  Lz4FrameDecompressor();
  ~Lz4FrameDecompressor() override;
  bool Decompress(const void* src, std::size_t size,
                  CompressionOutputStream* out) override;
  bool Decompress(const NoncontiguousBuffer& src,
                  CompressionOutputStream* out) override;

 private:
  bool Append(const void* buffer, std::size_t size);
  bool Flush();
  bool Init(CompressionOutputStream* out);

 private:
  CompressionOutputStream* out_ = nullptr;
  LZ4F_dctx* ctx_ = nullptr;
};

}  // namespace flare::compression

#endif  // FLARE_BASE_COMPRESSION_LZ4_H_
