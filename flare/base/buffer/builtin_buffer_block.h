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

#ifndef FLARE_BASE_BUFFER_BUILTIN_BUFFER_BLOCK_H_
#define FLARE_BASE_BUFFER_BUILTIN_BUFFER_BLOCK_H_

#include <array>
#include <chrono>
#include <limits>

#include "flare/base/align.h"
#include "flare/base/buffer/polymorphic_buffer.h"
#include "flare/base/object_pool.h"

namespace flare {

// "Native" buffer.
//
// Use `MakeNativeBufferBlock` to instantiate this class.
class alignas(hardware_destructive_interference_size) NativeBufferBlock
    : public PolymorphicBufferBlock {
 public:
  virtual char* mutable_data() noexcept = 0;
};

// Allocate a buffer block.
//
// Size of the buffer block is determined by GFlags on startup, see
// implementation for detail.
RefPtr<NativeBufferBlock> MakeNativeBufferBlock();

// This buffer references a non-owning memory region.
//
// The buffer creator is responsible for making sure the memory region
// referenced by this object is not mutated during the whole lifetime of this
// object.
//
// This class calls user's callback on destruction. This provides a way for the
// user to know when the buffer being referenced is safe to release.
template <class F>
class ReferencingBufferBlock : public PolymorphicBufferBlock,
                               private F /* EBO */ {
 public:
  explicit ReferencingBufferBlock(const void* ptr, std::size_t size,
                                  F&& completion_cb)
      : F(std::move(completion_cb)), ptr_(ptr), size_(size) {}
  ~ReferencingBufferBlock() { (*this)(); }

  const char* data() const noexcept override {
    return reinterpret_cast<const char*>(ptr_);
  }
  std::size_t size() const noexcept override { return size_; }

 private:
  const void* ptr_;
  std::size_t size_;
};

}  // namespace flare

#endif  // FLARE_BASE_BUFFER_BUILTIN_BUFFER_BLOCK_H_
