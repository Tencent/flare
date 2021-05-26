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

#include "flare/base/compression/gzip.h"

#include <algorithm>
#include <string>

#include "flare/base/compression/util.h"

namespace flare::compression {

FLARE_COMPRESSION_REGISTER_COMPRESSOR("gzip", GzipCompressor);
FLARE_COMPRESSION_REGISTER_DECOMPRESSOR("gzip", GzipDecompressor);

namespace {

// Calculate data compression rate according to already processed data
// adaptively.
inline double EstimateCompressionRate(const z_stream* stream,
                                      double default_value) {
  if (stream->total_in > 0) {
    double rate = static_cast<double>(stream->total_out) / stream->total_in;
    const double kMargin = 1.1;
    return rate * kMargin;
  }
  return default_value;
}

// The type of both `avail_in` and `avail_out` in z_stream are `uInt`,
// so we must restrict their value to be under 4G.
inline uint32_t RestrictAvailSize(size_t size) {
  return static_cast<uint32_t>(std::min<size_t>(UINT32_MAX, size));
}

// See document of inflateInit2 in zlib.h
constexpr int ZLIB_INIT_FLAG_GZIP = 16;

// The size to increase every time Z_BUF_ERROR returns.
constexpr int kOutBufferIncreaseSize = 32;

bool DoAppend(z_stream* stream, CompressionOutputStream* out,
              const void* buffer, std::size_t size, bool is_deflate,
              bool finish) {
  FLARE_CHECK(stream && out);

  if (!finish && (buffer == nullptr || size == 0)) {
    return true;
  }

  stream->next_in = reinterpret_cast<Bytef*>(const_cast<void*>(buffer));
  int code = Z_OK;
  int need_more_space_cnt = 0;
  std::string tmp_buffer;
  size_t left_size = size;
  while (left_size > 0 || need_more_space_cnt > 0 || finish) {
    stream->avail_in = RestrictAvailSize(left_size);
    auto current_avail_in = stream->avail_in;
    std::size_t size;
    if (!need_more_space_cnt) {
      // Try output stream first.
      void* data;
      if (!out->Next(&data, &size)) {
        return false;
      }
      stream->next_out = reinterpret_cast<Bytef*>(data);
    } else {
      // Output stream does not have enough continuous space.
      // We use string buffer.
      double rate = EstimateCompressionRate(stream, 0.5);
      tmp_buffer.resize(left_size * rate +
                        need_more_space_cnt * kOutBufferIncreaseSize);
      stream->next_out = reinterpret_cast<Bytef*>(tmp_buffer.data());
      size = tmp_buffer.size();
    }
    stream->avail_out = RestrictAvailSize(size);
    std::size_t current_avail_out = stream->avail_out;

    FLARE_CHECK_GE(stream->avail_in, 0,
                   "Avail_out should never be zero before the call");
    int flush_option = finish ? Z_FINISH : Z_NO_FLUSH;
    if (is_deflate) {
      code = deflate(stream, flush_option);
    } else {
      code = inflate(stream, flush_option);
    }
    if (code == Z_BUF_ERROR) {
      if (need_more_space_cnt == 0) {
        out->BackUp(size);
      }
      ++need_more_space_cnt;
      continue;
    }
    if (code < 0) {
      FLARE_LOG_ERROR_EVERY_SECOND("error code {}", code);
      break;
    }

    left_size -= current_avail_in - stream->avail_in;
    if (need_more_space_cnt == 0) {
      out->BackUp(stream->avail_out);
    } else {
      if (FLARE_UNLIKELY(!CopyDataToCompressionOutputStream(
              out, tmp_buffer.data(), current_avail_out - stream->avail_out))) {
        return false;
      }
      need_more_space_cnt = 0;
    }

    if (code == Z_STREAM_END) {
      return true;
    }
  }

  return code == Z_OK;
}

}  // namespace

bool GzipCompressor::Compress(const void* src, std::size_t size,
                              CompressionOutputStream* out) {
  bool ok = Init(out);
  ok &= Append(src, size);
  ok &= Flush();
  out_ = nullptr;
  return ok;
}

bool GzipCompressor::Compress(const NoncontiguousBuffer& bytes,
                              CompressionOutputStream* out) {
  bool ok = Init(out);
  std::size_t left = bytes.ByteSize();
  for (auto iter = bytes.begin(); ok && iter != bytes.end() && left; ++iter) {
    auto len = std::min(left, iter->size());
    ok &= Append(iter->data(), len);
    left -= len;
  }
  ok &= Flush();
  out_ = nullptr;
  return ok;
}

bool GzipCompressor::Init(CompressionOutputStream* out) {
  FLARE_CHECK(!out_);
  stream_ = std::make_unique<z_stream>();
  int code = deflateInit2(stream_.get(), Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                          MAX_WBITS + ZLIB_INIT_FLAG_GZIP, 8,
                          Z_DEFAULT_STRATEGY) != Z_OK;
  if (code != Z_OK) {
    FLARE_LOG_ERROR_EVERY_SECOND("DeflateInit2 error {}", code);
    stream_ = nullptr;
  } else {
    out_ = out;
  }
  return code == Z_OK;
}

bool GzipCompressor::Release() {
  FLARE_CHECK(stream_);
  int code = deflateEnd(stream_.get());
  if (code != Z_OK) {
    FLARE_LOG_WARNING_EVERY_SECOND("DeflateEnd fail with code {}", code);
  }
  return code == Z_OK;
}

bool GzipCompressor::Append(const void* buffer, std::size_t size) {
  return DoAppend(stream_.get(), out_, buffer, size, true, false);
}

bool GzipCompressor::Flush() {
  FLARE_CHECK(out_);
  if (!DoAppend(stream_.get(), out_, nullptr, 0, true, true)) {
    return false;
  }
  return Release();
}

bool GzipDecompressor::Decompress(const void* src, std::size_t size,
                                  CompressionOutputStream* out) {
  bool ok = Init(out);
  ok &= Append(src, size);
  ok &= Flush();
  out_ = nullptr;
  return ok;
}

bool GzipDecompressor::Decompress(const NoncontiguousBuffer& compressed,
                                  CompressionOutputStream* out) {
  bool ok = Init(out);
  std::size_t left = compressed.ByteSize();
  for (auto iter = compressed.begin(); ok && iter != compressed.end() && left;
       ++iter) {
    auto len = std::min(left, iter->size());
    ok &= Append(iter->data(), len);
    left -= len;
  }
  ok &= Flush();
  out_ = nullptr;
  return ok;
}

bool GzipDecompressor::Init(CompressionOutputStream* out) {
  FLARE_CHECK(!out_);
  stream_ = std::make_unique<z_stream>();
  int code = inflateInit2(stream_.get(), MAX_WBITS + ZLIB_INIT_FLAG_GZIP);
  if (code != Z_OK) {
    FLARE_LOG_ERROR_EVERY_SECOND("InflateInit2 error {}", code);
    stream_ = nullptr;
  } else {
    out_ = out;
  }
  return code == Z_OK;
}

bool GzipDecompressor::Release() {
  FLARE_CHECK(stream_);
  int code = inflateEnd(stream_.get());
  if (code != Z_OK) {
    FLARE_LOG_WARNING_EVERY_SECOND("InflateEnd fail with code {}", code);
  }
  return code == Z_OK;
}

bool GzipDecompressor::Flush() {
  FLARE_CHECK(out_);
  out_ = nullptr;
  return Release();
}

bool GzipDecompressor::Append(const void* buffer, std::size_t size) {
  return DoAppend(stream_.get(), out_, buffer, size, false, false);
}

}  // namespace flare::compression
