// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_BASE_BUFFER_ZERO_COPY_STREAM_H_
#define FLARE_BASE_BUFFER_ZERO_COPY_STREAM_H_

// This file provides helper classes for you to serialize / deserialize Protocol
// Buffers to / from `NoncontiguousBuffer`.

#include "google/protobuf/io/zero_copy_stream.h"

#include "flare/base/buffer.h"

namespace flare {

// This class provides you a way to parse Protocol Buffers from
// `NoncontiguousBuffer` without flattening the later.
//
// The buffer given to this class is consumed. You need to make a copy before
// hand if that's not what you want.
//
// Usage:
//
//   NoncontiguousBufferInputStream nbis(&buffer);
//   FLARE_CHECK(message.ParseFromZeroCopyStream(&nbis));
//   nbis.Flush();
//
// You may also want to check UT for usage example.
class NoncontiguousBufferInputStream
    : public google::protobuf::io::ZeroCopyInputStream {
 public:
  explicit NoncontiguousBufferInputStream(NoncontiguousBuffer* ref);
  ~NoncontiguousBufferInputStream();

  // Synchronizes with `NoncontiguousBuffer`. You must call this method before
  // touching the buffer again.
  //
  // On destruction, the stream is automatically synchronized with the buffer.
  void Flush();

  bool Next(const void** data, int* size) override;
  void BackUp(int count) override;
  bool Skip(int count) override;
  google::protobuf::int64 ByteCount() const override;

 private:
  void PerformPendingSkips();

 private:
  std::size_t skip_before_read_{0};
  std::size_t read_{0};
  NoncontiguousBuffer* buffer_;
};

// This class provides you a way to serialize Protocol Buffers into
// `NoncontiguousBuffer`.
//
// Usage:
//
//   NoncontiguousBufferOutputStream nbos;
//   FLARE_CHECK(message.SerializeToZeroCopyStream(&nbos));
//   nbos.Flush();
//
// You may also want to check UT for usage example.
class NoncontiguousBufferOutputStream
    : public google::protobuf::io::ZeroCopyOutputStream {
 public:
  explicit NoncontiguousBufferOutputStream(NoncontiguousBufferBuilder* builder);
  ~NoncontiguousBufferOutputStream();

  // Flush internal state. Must be called before touching `builder`.
  //
  // On destruction, we automatically synchronizes with `builder`.
  void Flush();

  bool Next(void** data, int* size) override;
  void BackUp(int count) override;
  google::protobuf::int64 ByteCount() const override;

 private:
  std::size_t using_bytes_{};
  NoncontiguousBufferBuilder* builder_;
};

}  // namespace flare

#endif  // FLARE_BASE_BUFFER_ZERO_COPY_STREAM_H_
