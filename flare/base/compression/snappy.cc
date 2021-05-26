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

#include "flare/base/compression/snappy.h"

#include <string>

#include "thirdparty/snappy/snappy-sinksource.h"

#include "flare/base/compression/util.h"

namespace flare::compression {

FLARE_COMPRESSION_REGISTER_COMPRESSOR("snappy", SnappyCompressor);
FLARE_COMPRESSION_REGISTER_DECOMPRESSOR("snappy", SnappyDecompressor);

namespace {

// Souce
class NoncontiguousBufferSource : public snappy::Source {
 public:
  explicit NoncontiguousBufferSource(const NoncontiguousBuffer& nb) : nb_(nb) {}
  size_t Available() const override { return nb_.ByteSize(); }
  const char* Peek(size_t* len) override {
    if (!Available()) {
      *len = 0;
      return nullptr;
    }
    auto&& c = nb_.FirstContiguous();
    *len = c.size();
    return c.data();
  }
  void Skip(size_t n) override {
    // Cut instead of Skip !!!
    nb_.Cut(n);
  }

 private:
  NoncontiguousBuffer nb_;
};

using ByteArraySource = snappy::ByteArraySource;

// Sink
class CompressionOutputStreamSink : public snappy::Sink {
 public:
  explicit CompressionOutputStreamSink(CompressionOutputStream* out)
      : out_(out) {}

  // Append and GetAppendBuffer will be used for compress.
  // They will be called in turn, first GetAppendBuffer, then Append, then
  // GetAppendBuffer...
  void Append(const char* data, size_t n) override {
    if (data == next_data_) {
      // Snappy used our buffer.
      FLARE_CHECK_LE(n, next_size_,
                     "Append size should no more than next size.");
      out_->BackUp(next_size_ - n);
    } else {
      out_->BackUp(next_size_);
      // Snappy used scratch.
      FLARE_CHECK(CopyDataToCompressionOutputStream(out_, data, n),
                  "CopyDataToCompressionOutputStream should success.");
    }
  }

  char* GetAppendBuffer(size_t length, char* scratch) override {
    FLARE_CHECK(out_->Next(&next_data_, &next_size_),
                "Output stream can't offer next buffer.");
    if (next_size_ >= length) {
      return reinterpret_cast<char*>(next_data_);
    } else {
      return scratch;
    }
  }

  // AppendAndTakeOwnership and GetAppendBufferVariable will be used for
  // uncompress for snappy 1.1.8.
  // They are more convenient and effective(Avoid some copies).
  // But upgrading to 1.1.8 will affect the stability of other services(Why?).
  // So We can't use these two methods now.
  //
  // GetAppendBufferVariable will be called once and then AppendAndTakeOwnership
  // may be called once if snappy used our buffer, else it will use scartch, and
  // GetAppendBufferVariable may be called multi times.
  /*void AppendAndTakeOwnership(char* data, size_t n,
                              void (*deleter)(void*, const char*, size_t),
                              void* deleter_arg) override {
    if (data == next_data_) {
      // Snappy used our buffer.
      FLARE_CHECK_LE(n, next_size_,
                     "Append size should no more than next size.");
      out_->BackUp(next_size_ - n);
    } else {
      out_->BackUp(next_size_);
      // Reset next_size_ to prevent multiple Backup !
      next_size_ = 0;

      // Snappy used scratch.
      FLARE_CHECK(CopyDataToCompressionOutputStream(out_, data, n),
                  "CopyDataToCompressionOutputStream should success.");
      (*deleter)(deleter_arg, data, n);
    }
  }

  char* GetAppendBufferVariable(size_t min_size, size_t desired_size_hint,
                                char* scratch, size_t scratch_size,
                                size_t* allocated_size) override {
    FLARE_CHECK(out_->Next(&next_data_, &next_size_),
                "Output stream can't offer next buffer.");
    if (next_size_ >= min_size) {
      *allocated_size = next_size_;
      return reinterpret_cast<char*>(next_data_);
    } else {
      *allocated_size = scratch_size;
      return scratch;
    }
  }*/

 private:
  CompressionOutputStream* out_;
  void* next_data_ = nullptr;
  std::size_t next_size_ = 0;
};

bool Uncompress(snappy::Source* source, const char* head, std::size_t limit,
                CompressionOutputStream* out) {
  std::size_t uncompressed_length;
  if (!snappy::GetUncompressedLength(head, limit, &uncompressed_length)) {
    FLARE_LOG_WARNING_EVERY_SECOND("GetUncompressedLength error");
    return false;
  }

  std::string uncompressed;
  uncompressed.resize(uncompressed_length);
  if (!snappy::RawUncompress(source, uncompressed.data())) {
    FLARE_LOG_WARNING_EVERY_SECOND("Uncompress error");
    return false;
  }

  if (!CopyDataToCompressionOutputStream(out, uncompressed.data(),
                                         uncompressed_length)) {
    FLARE_LOG_WARNING_EVERY_SECOND("CopyDataToCompressionOutputStream error");
    return false;
  }

  return true;
}

}  // namespace

bool SnappyCompressor::Compress(const void* src, std::size_t size,
                                CompressionOutputStream* out) {
  ByteArraySource source(reinterpret_cast<const char*>(src), size);
  CompressionOutputStreamSink sink(out);
  snappy::Compress(&source, &sink);
  return true;
}

bool SnappyCompressor::Compress(const NoncontiguousBuffer& nb,
                                CompressionOutputStream* out) {
  NoncontiguousBufferSource source(nb);
  CompressionOutputStreamSink sink(out);
  snappy::Compress(&source, &sink);
  return true;
}

bool SnappyDecompressor::Decompress(const void* s, std::size_t size,
                                    CompressionOutputStream* out) {
  const char* src = reinterpret_cast<const char*>(s);
  ByteArraySource source(src, size);
  return Uncompress(&source, src, size, out);
}

bool SnappyDecompressor::Decompress(const NoncontiguousBuffer& nb,
                                    CompressionOutputStream* out) {
  // GetUncompressedLength needs varint32 to get length
  // So we need at most 5 bytes.
  std::string head = FlattenSlow(nb, 5);
  NoncontiguousBufferSource source(nb);
  return Uncompress(&source, head.data(), head.size(), out);
}

}  // namespace flare::compression
