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

#ifndef FLARE_BASE_COMPRESSION_COMPRESSION_H_
#define FLARE_BASE_COMPRESSION_COMPRESSION_H_

#include <memory>

#include "flare/base/buffer.h"
#include "flare/base/dependency_registry.h"
#include "flare/base/function.h"

namespace flare {

// Abstract interface similar to ZeroCopyOutputStream and designed to minimize
// copying for compression.
class CompressionOutputStream {
 public:
  virtual ~CompressionOutputStream() = default;
  // Obtains a buffer into which data can be written.  Any data written
  // into this buffer will eventually (maybe instantly, maybe later on)
  // be written to the output.
  virtual bool Next(void** data, std::size_t* size) noexcept = 0;
  // Backs up a number of bytes, so that the end of the last buffer returned
  // by Next() is not actually written.  This is needed when you finish
  // writing all the data you want to write, but the last buffer was bigger
  // than you needed.  You don't want to write a bunch of garbage after the
  // end of your data, so you use BackUp() to back up.
  virtual void BackUp(std::size_t count) noexcept = 0;
};

// Class for compression.
class Compressor {
 public:
  virtual ~Compressor() = default;

  // Not thread-safe.
  virtual bool Compress(const void* src, std::size_t size,
                        CompressionOutputStream* out) = 0;
  virtual bool Compress(const NoncontiguousBuffer& src,
                        CompressionOutputStream* out) = 0;
};

// Class for decompression.
class Decompressor {
 public:
  virtual ~Decompressor() = default;

  // Not thread-safe.
  virtual bool Decompress(const void* src, std::size_t size,
                          CompressionOutputStream* out) = 0;
  virtual bool Decompress(const NoncontiguousBuffer& src,
                          CompressionOutputStream* out) = 0;
};

FLARE_DECLARE_CLASS_DEPENDENCY_REGISTRY(compressor_registry, Compressor);
FLARE_DECLARE_CLASS_DEPENDENCY_REGISTRY(decompressor_registry, Decompressor);

}  // namespace flare

#define FLARE_COMPRESSION_REGISTER_COMPRESSOR(Name, Implementation) \
  FLARE_REGISTER_CLASS_DEPENDENCY(flare::compressor_registry, Name, \
                                  Implementation);

#define FLARE_COMPRESSION_REGISTER_DECOMPRESSOR(Name, Implementation) \
  FLARE_REGISTER_CLASS_DEPENDENCY(flare::decompressor_registry, Name, \
                                  Implementation);

#endif  // FLARE_BASE_COMPRESSION_COMPRESSION_H_
