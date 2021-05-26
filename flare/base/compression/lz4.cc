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

#include "flare/base/compression/lz4.h"

#include <algorithm>
#include <string>

#include "flare/base/compression/util.h"

namespace flare::compression {

FLARE_COMPRESSION_REGISTER_COMPRESSOR("lz4-frame", Lz4FrameCompressor);
FLARE_COMPRESSION_REGISTER_DECOMPRESSOR("lz4-frame", Lz4FrameDecompressor);

// Lz4FrameCompressor

Lz4FrameCompressor::Lz4FrameCompressor() {
  auto r = LZ4F_createCompressionContext(&ctx_, LZ4F_VERSION);
  FLARE_CHECK(!LZ4F_isError(r), "Failed to create context: error {}",
              LZ4F_getErrorName(r));
}

bool Lz4FrameCompressor::DoCompress(std::size_t size, DoCompressFunc f) {
  void* next_data;
  std::size_t next_size;
  if (FLARE_UNLIKELY(!out_->Next(&next_data, &next_size))) {
    return false;
  }
  std::string tmp_buffer;
  bool use_buffer = false;
  if (next_size < size) {
    use_buffer = true;
    out_->BackUp(next_size);
    tmp_buffer.resize(size);
    next_data = tmp_buffer.data();
    next_size = size;
  }

  std::size_t n = f(next_data, next_size);
  if (LZ4F_isError(n)) {
    FLARE_LOG_ERROR("Failed to compress: error {}", LZ4F_getErrorName(n));
    return false;
  }
  if (use_buffer) {
    if (FLARE_UNLIKELY(
            !CopyDataToCompressionOutputStream(out_, next_data, n))) {
      return false;
    }
  } else {
    out_->BackUp(next_size - n);
  }
  return true;
}

bool Lz4FrameCompressor::Compress(const void* src, std::size_t size,
                                  CompressionOutputStream* out) {
  bool ok = Init(out);
  ok &= Append(src, size);
  ok &= Flush();
  out_ = nullptr;
  return ok;
}

bool Lz4FrameCompressor::Compress(const NoncontiguousBuffer& nb,
                                  CompressionOutputStream* out) {
  bool ok = Init(out);
  std::size_t left = nb.ByteSize();
  for (auto iter = nb.begin(); ok && iter != nb.end() && left; ++iter) {
    auto len = std::min(left, iter->size());
    ok &= Append(iter->data(), len);
    left -= len;
  }
  ok &= Flush();
  out_ = nullptr;
  return ok;
}

bool Lz4FrameCompressor::Init(CompressionOutputStream* out) {
  out_ = out;
  if (!DoCompress(LZ4F_HEADER_SIZE_MAX, [this](void* data, std::size_t size) {
        return LZ4F_compressBegin(ctx_, data, size, nullptr);
      })) {
    out_ = nullptr;
    return false;
  }
  return true;
}

Lz4FrameCompressor::~Lz4FrameCompressor() {
  if (ctx_) LZ4F_freeCompressionContext(ctx_);
}

bool Lz4FrameCompressor::Append(const void* buffer, std::size_t size) {
  FLARE_CHECK(out_);
  if (size == 0) {
    return true;
  }
  return DoCompress(LZ4F_compressBound(size, nullptr),
                    [=, this](void* data, std::size_t dst_size) {
                      return LZ4F_compressUpdate(ctx_, data, dst_size, buffer,
                                                 size, nullptr);
                    });
}

bool Lz4FrameCompressor::Flush() {
  FLARE_CHECK(out_);
  // When srcSize==0, LZ4F_compressBound() provides an upper bound for
  // LZ4F_flush() and LZ4F_compressEnd() operations.
  if (DoCompress(LZ4F_compressBound(0, nullptr),
                 [this](void* data, std::size_t size) {
                   return LZ4F_compressEnd(ctx_, data, size, nullptr);
                 })) {
    return true;
  }
  return false;
}

// Lz4FrameDecompressor

Lz4FrameDecompressor::Lz4FrameDecompressor() {
  auto r = LZ4F_createDecompressionContext(&ctx_, 100);
  FLARE_CHECK(!LZ4F_isError(r), "Failed to create context: error {}",
              LZ4F_getErrorName(r));
}

Lz4FrameDecompressor::~Lz4FrameDecompressor() {
  if (ctx_) LZ4F_freeDecompressionContext(ctx_);
}

bool Lz4FrameDecompressor::Init(CompressionOutputStream* out) {
  FLARE_CHECK(!out_);
  out_ = out;
  return true;
}

bool Lz4FrameDecompressor::Decompress(const void* src, std::size_t size,
                                      CompressionOutputStream* out) {
  bool ok = Init(out);
  ok &= Append(src, size);
  ok &= Flush();
  out_ = nullptr;
  return ok;
}

bool Lz4FrameDecompressor::Decompress(const NoncontiguousBuffer& nb,
                                      CompressionOutputStream* out) {
  bool success = Init(out);
  std::size_t left = nb.ByteSize();
  for (auto iter = nb.begin(); success && iter != nb.end() && left; ++iter) {
    auto len = std::min(left, iter->size());
    success &= Append(iter->data(), len);
    left -= len;
  }
  success &= Flush();
  out_ = nullptr;
  return success;
}

bool Lz4FrameDecompressor::Append(const void* buffer, std::size_t size) {
  FLARE_CHECK(out_);
  if (size == 0) {
    return true;
  }
  const char* src_cur = reinterpret_cast<const char*>(buffer);
  const char* src_end = reinterpret_cast<const char*>(buffer) + size;
  std::size_t ret = 1;
  std::size_t src_size = size;
  while (src_cur < src_end) {
    void* next_data;
    std::size_t next_size;
    if (FLARE_UNLIKELY(!out_->Next(&next_data, &next_size))) {
      return false;
    }
    auto org_next_size = next_size;
    ret = LZ4F_decompress(ctx_, next_data, &next_size, src_cur, &src_size,
                          /* LZ4F_decompressOptions_t */ nullptr);
    if (LZ4F_isError(ret)) {
      FLARE_LOG_ERROR("Failed to decompress: error {}", LZ4F_getErrorName(ret));
      return false;
    }
    out_->BackUp(org_next_size - next_size);
    src_cur += src_size;
    src_size = src_end - src_cur;
  }
  return true;
}

bool Lz4FrameDecompressor::Flush() {
  FLARE_CHECK(out_);
  return true;
}

}  // namespace flare::compression
