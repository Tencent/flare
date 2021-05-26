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

#include "flare/base/buffer.h"

#include <algorithm>
#include <utility>

#include "flare/base/logging.h"

namespace flare {

namespace {

// This buffer owns a container.
template <class T>
class OwningBufferBlock : public PolymorphicBufferBlock {
 public:
  explicit OwningBufferBlock(T storage) : storage_(std::move(storage)) {}

  const char* data() const noexcept override {
    return reinterpret_cast<const char*>(storage_.data());
  }
  std::size_t size() const noexcept override { return storage_.size(); }

 private:
  T storage_;
};

}  // namespace

namespace detail {

void FlattenToSlowSlow(const NoncontiguousBuffer& nb, void* buffer,
                       std::size_t size) {
  FLARE_CHECK(nb.ByteSize() >= size, "No enough data.");
  std::size_t copied = 0;
  for (auto iter = nb.begin(); iter != nb.end() && copied != size; ++iter) {
    auto len = std::min(size - copied, iter->size());
    memcpy(reinterpret_cast<char*>(buffer) + copied, iter->data(), len);
    copied += len;
  }
}

}  // namespace detail

NoncontiguousBuffer::NoncontiguousBuffer(const NoncontiguousBuffer& nb)
    : byte_size_(nb.byte_size_) {
  for (auto&& e : nb.buffers_) {
    // Exception unsafe.
    auto b = object_pool::Get<PolymorphicBuffer>().Leak();
    *b = e;
    buffers_.push_back(b);
  }
}

NoncontiguousBuffer& NoncontiguousBuffer::operator=(
    const NoncontiguousBuffer& nb) {
  if (FLARE_UNLIKELY(&nb == this)) {
    return *this;
  }
  Clear();
  byte_size_ = nb.byte_size_;
  for (auto&& e : nb.buffers_) {
    // Exception unsafe.
    auto b = object_pool::Get<PolymorphicBuffer>().Leak();
    *b = e;
    buffers_.push_back(b);
  }
  return *this;
}

void NoncontiguousBuffer::SkipSlow(std::size_t bytes) noexcept {
  byte_size_ -= bytes;

  while (bytes) {
    auto&& first = buffers_.front();
    auto os = std::min(bytes, first.size());
    if (os == first.size()) {
      object_pool::Put<PolymorphicBuffer>(buffers_.pop_front());
    } else {
      FLARE_DCHECK_LT(os, first.size());
      first.Skip(os);
    }
    bytes -= os;
  }
}

void NoncontiguousBuffer::ClearSlow() noexcept {
  byte_size_ = 0;
  while (!buffers_.empty()) {
    object_pool::Put<PolymorphicBuffer>(buffers_.pop_front());
  }
}

void NoncontiguousBufferBuilder::InitializeNextBlock() {
  if (current_) {
    FLARE_CHECK(SizeAvailable());
    return;  // Nothing to do then.
  }
  current_ = MakeNativeBufferBlock();
  used_ = 0;
}

// Move the buffer block we're working on into the non-contiguous buffer we're
// building.
void NoncontiguousBufferBuilder::FlushCurrentBlock() {
  if (!used_) {
    return;  // The current block is clean, no need to flush it.
  }
  nb_.Append(PolymorphicBuffer(std::move(current_), 0, used_));
  used_ = 0;  // Not strictly needed as it'll be re-initialized by
              // `InitializeNextBlock()` anyway.
}

void NoncontiguousBufferBuilder::AppendSlow(const void* ptr,
                                            std::size_t length) {
  while (length) {
    auto copying = std::min(length, SizeAvailable());
    memcpy(data(), ptr, copying);
    MarkWritten(copying);
    ptr = static_cast<const char*>(ptr) + copying;
    length -= copying;
  }
}

void NoncontiguousBufferBuilder::AppendCopy(const NoncontiguousBuffer& buffer) {
  for (auto&& e : buffer) {
    Append(e.data(), e.size());
  }
}

NoncontiguousBuffer CreateBufferSlow(std::string_view s) {
  NoncontiguousBufferBuilder nbb;
  nbb.Append(s);
  return nbb.DestructiveGet();
}

NoncontiguousBuffer CreateBufferSlow(const void* ptr, std::size_t size) {
  NoncontiguousBufferBuilder nbb;
  nbb.Append(ptr, size);
  return nbb.DestructiveGet();
}

std::string FlattenSlow(const NoncontiguousBuffer& nb, std::size_t max_bytes) {
  max_bytes = std::min(max_bytes, nb.ByteSize());
  std::string rc;
  std::size_t left = max_bytes;
  rc.reserve(max_bytes);
  for (auto iter = nb.begin(); iter != nb.end() && left; ++iter) {
    auto len = std::min(left, iter->size());
    rc.append(iter->data(), len);
    left -= len;
  }
  return rc;
}

std::string FlattenSlowUntil(const NoncontiguousBuffer& nb,
                             std::string_view delim, std::size_t max_bytes) {
  if (nb.Empty()) {
    return {};
  }

  // Given that our block is large enough, and the caller should not be
  // expecting too much data (since this method is slow), it's likely we even
  // don't have to fully copy the first block. So we optimize for this case.
  std::string_view current(nb.FirstContiguous().data(),
                           nb.FirstContiguous().size());
  if (auto pos = current.find(delim); pos != std::string_view::npos) {
    auto expected_bytes = std::min(pos + delim.size(), max_bytes);
    return std::string(nb.FirstContiguous().data(),
                       nb.FirstContiguous().data() + expected_bytes);
  }

  // Slow path otherwise.
  std::string rc;
  for (auto iter = nb.begin(); iter != nb.end() && rc.size() < max_bytes;
       ++iter) {
    auto old_len = rc.size();
    rc.append(iter->data(), iter->size());
    if (auto pos = rc.find(delim, old_len - std::min(old_len, delim.size()));
        pos != std::string::npos) {
      rc.erase(rc.begin() + pos + delim.size(), rc.end());
      break;
    }
  }
  if (rc.size() > max_bytes) {
    rc.erase(rc.begin() + max_bytes, rc.end());
  }
  return rc;
}

PolymorphicBuffer MakeReferencingBuffer(const void* ptr, std::size_t size) {
  return MakeReferencingBuffer(ptr, size, [] {});
}

PolymorphicBuffer MakeForeignBuffer(std::string buffer) {
  auto size = buffer.size();
  return PolymorphicBuffer(
      MakeRefCounted<OwningBufferBlock<std::string>>(std::move(buffer)), 0,
      size);
}

template <class T>
PolymorphicBuffer MakeForeignBuffer(std::vector<T> buffer) {
  auto size = buffer.size() * sizeof(T);
  return PolymorphicBuffer(
      MakeRefCounted<OwningBufferBlock<std::vector<T>>>(std::move(buffer)), 0,
      size);
}

#define INSTANTIATE_MAKE_FOREIGN_BUFFER(type) \
  template PolymorphicBuffer MakeForeignBuffer<type>(std::vector<type> buffer);

INSTANTIATE_MAKE_FOREIGN_BUFFER(std::byte);
INSTANTIATE_MAKE_FOREIGN_BUFFER(char);                // NOLINT
INSTANTIATE_MAKE_FOREIGN_BUFFER(signed char);         // NOLINT
INSTANTIATE_MAKE_FOREIGN_BUFFER(signed short);        // NOLINT
INSTANTIATE_MAKE_FOREIGN_BUFFER(signed int);          // NOLINT
INSTANTIATE_MAKE_FOREIGN_BUFFER(signed long);         // NOLINT
INSTANTIATE_MAKE_FOREIGN_BUFFER(signed long long);    // NOLINT
INSTANTIATE_MAKE_FOREIGN_BUFFER(unsigned char);       // NOLINT
INSTANTIATE_MAKE_FOREIGN_BUFFER(unsigned short);      // NOLINT
INSTANTIATE_MAKE_FOREIGN_BUFFER(unsigned int);        // NOLINT
INSTANTIATE_MAKE_FOREIGN_BUFFER(unsigned long);       // NOLINT
INSTANTIATE_MAKE_FOREIGN_BUFFER(unsigned long long);  // NOLINT
INSTANTIATE_MAKE_FOREIGN_BUFFER(float);
INSTANTIATE_MAKE_FOREIGN_BUFFER(double);
INSTANTIATE_MAKE_FOREIGN_BUFFER(long double);

}  // namespace flare
