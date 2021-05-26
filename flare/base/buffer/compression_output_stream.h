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

#ifndef FLARE_BASE_BUFFER_COMPRESSION_OUTPUT_STREAM_H_
#define FLARE_BASE_BUFFER_COMPRESSION_OUTPUT_STREAM_H_

#include "flare/base/compression/compression.h"

namespace flare {

class NoncontiguousBufferCompressionOutputStream
    : public CompressionOutputStream {
 public:
  explicit NoncontiguousBufferCompressionOutputStream(
      NoncontiguousBufferBuilder* builder);
  ~NoncontiguousBufferCompressionOutputStream() override;

  // Flush internal state. Must be called before touching `builder`.
  //
  // On destruction, we automatically synchronizes with `builder`.
  void Flush();

  bool Next(void** data, std::size_t* size) noexcept override;
  void BackUp(std::size_t count) noexcept override;

 private:
  std::size_t using_bytes_{};
  NoncontiguousBufferBuilder* builder_;
};

}  // namespace flare

#endif  // FLARE_BASE_BUFFER_COMPRESSION_OUTPUT_STREAM_H_
