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

#ifndef FLARE_BASE_BUFFER_H_
#define FLARE_BASE_BUFFER_H_

// Inspired by `brpc/butil/iobuf.h`.
//
// @sa: https://github.com/apache/incubator-brpc/blob/master/src/butil/iobuf.h

#include <cstddef>

#include <algorithm>
#include <limits>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "flare/base/buffer/builtin_buffer_block.h"
#include "flare/base/buffer/polymorphic_buffer.h"
#include "flare/base/internal/singly_linked_list.h"
#include "flare/base/logging.h"

// We use `char*` instead of `void*` here so that doing arithmetic on pointers
// would be easier.

namespace flare {

namespace detail {

// Similar to `std::data()`, but the return type must be convertible to const
// char*.
template <class T>
constexpr auto data(const T& c)
    -> std::enable_if_t<std::is_convertible_v<decltype(c.data()), const char*>,
                        const char*> {
  return c.data();
}

template <std::size_t N>
constexpr auto data(const char (&c)[N]) {
  return c;
}

template <class T>
constexpr std::size_t size(const T& c) {
  return c.size();
}

// Similar to `std::size` except for it returns number of chars instead of array
// size (which is 1 greater than number of chars.)
template <std::size_t N>
constexpr std::size_t size(const char (&c)[N]) {
  if (N == 0) {
    return 0;
  }
  return N - (c[N - 1] == 0);
}

}  // namespace detail

// Internally a `NoncontiguousBuffer` consists of multiple `PolymorphicBuffer`s.
class NoncontiguousBuffer {
  using LinkedBuffers =
      internal::SinglyLinkedList<PolymorphicBuffer, &PolymorphicBuffer::chain>;

 public:
  using iterator = LinkedBuffers::iterator;
  using const_iterator = LinkedBuffers::const_iterator;

  constexpr NoncontiguousBuffer() = default;

  // Copyable & movable.
  //
  // It's relatively cheap to copy this object but move still performs better.
  NoncontiguousBuffer(const NoncontiguousBuffer& nb);
  NoncontiguousBuffer& operator=(const NoncontiguousBuffer& nb);
  NoncontiguousBuffer(NoncontiguousBuffer&& nb) noexcept
      : byte_size_(std::exchange(nb.byte_size_, 0)),
        buffers_(std::move(nb.buffers_)) {}
  NoncontiguousBuffer& operator=(NoncontiguousBuffer&& nb) noexcept {
    if (FLARE_UNLIKELY(&nb == this)) {
      return *this;
    }
    Clear();
    std::swap(byte_size_, nb.byte_size_);
    buffers_ = std::move(nb.buffers_);
    return *this;
  }

  ~NoncontiguousBuffer() { Clear(); }

  // Returns first "contiguous" part of this buffer.
  //
  // Precondition: !Empty().
  std::string_view FirstContiguous() const noexcept {
    FLARE_DCHECK(!Empty());
    auto&& first = buffers_.front();
    return std::string_view(first.data(), first.size());
  }

  // `bytes` can be greater than `FirstContiguous()->size()`, in this case
  // multiple buffer blocks are dropped.
  void Skip(std::size_t bytes) noexcept {
    FLARE_DCHECK_LE(bytes, ByteSize());
    if (FLARE_UNLIKELY(bytes == 0)) {
      // NOTHING.
    } else if (bytes < buffers_.front().size()) {
      buffers_.front().Skip(bytes);
      byte_size_ -= bytes;
    } else {
      SkipSlow(bytes);
    }
  }

  // Cut off first `bytes` bytes. That is, the first `bytes` bytes are removed
  // from `*this` and returned to the caller. `bytes` could be larger than
  // `FirstContiguous().size()`.
  //
  // `bytes` may not be greater than `ByteSize()`. Otherwise the behavior is
  // undefined.
  //
  // FIXME: Exception safety.
  NoncontiguousBuffer Cut(std::size_t bytes) {
    FLARE_DCHECK_LE(bytes, ByteSize());

    NoncontiguousBuffer rc;
    auto left = bytes;

    while (left && left >= buffers_.front().size()) {
      left -= buffers_.front().size();
      rc.buffers_.push_back(buffers_.pop_front());
    }

    if (FLARE_LIKELY(left)) {
      auto ncb =
          object_pool::Get<PolymorphicBuffer>().Leak();  // Exception unsafe.
      *ncb = buffers_.front();
      ncb->set_size(left);
      rc.buffers_.push_back(ncb);
      buffers_.front().Skip(left);
    }
    rc.byte_size_ = bytes;
    byte_size_ -= bytes;
    return rc;
  }

  void Append(PolymorphicBuffer buffer) {
    if (FLARE_UNLIKELY(buffer.size() == 0)) {  // Why would you do this?
      return;
    }
    auto block = object_pool::Get<PolymorphicBuffer>();
    *block = std::move(buffer);
    byte_size_ += block->size();
    buffers_.push_back(block.Leak());
  }

  void Append(NoncontiguousBuffer buffer) {
    byte_size_ += std::exchange(buffer.byte_size_, 0);
    buffers_.splice(std::move(buffer.buffers_));
  }

  // Total size of all buffers blocks.
  std::size_t ByteSize() const noexcept { return byte_size_; }

  bool Empty() const noexcept {
    FLARE_DCHECK_EQ(buffers_.empty(), !byte_size_);
    return !byte_size_;
  }

  void Clear() noexcept {
    if (!Empty()) {
      ClearSlow();
    }
  }

  // Non-mutating traversal.
  //
  // It's guaranteed that all elements are non-empty (i.e, their sizes are all
  // non-zero.).
  auto begin() const noexcept { return buffers_.begin(); }
  auto end() const noexcept { return buffers_.end(); }

 private:
  void SkipSlow(std::size_t bytes) noexcept;
  void ClearSlow() noexcept;

 private:
  std::size_t byte_size_{};
  LinkedBuffers buffers_;
};

// This class builds `NoncontiguousBuffer`.
class NoncontiguousBufferBuilder {
  // If `Append` is called with a buffer smaller than this threshold, it might
  // get copied even if technically a zero-copy mechanism is possible.
  //
  // This helps reduce internal memory fragmentation.
  inline static constexpr auto kAppendViaCopyThreshold = 128;

 public:
  NoncontiguousBufferBuilder() { InitializeNextBlock(); }

  // Get a pointer for writing. It's size is available at `SizeAvailable()`.
  char* data() const noexcept { return current_->mutable_data() + used_; }

  // Space available in buffer returned by `data()`.
  std::size_t SizeAvailable() const noexcept {
    return current_->size() - used_;
  }

  // Mark `bytes` bytes as written (i.e., advance `data()` and reduce `size()`).
  //
  // New internal buffer is allocated if the current one is saturated (i.e,
  // `bytes` == `size()`.).
  void MarkWritten(std::size_t bytes) {
    FLARE_DCHECK_LE(bytes, SizeAvailable(), "You're overflowing the buffer.");
    used_ += bytes;
    if (FLARE_UNLIKELY(!SizeAvailable())) {
      FlushCurrentBlock();
      InitializeNextBlock();
    }
  }

  // Reserve a contiguous block of bytes to be overwritten later.
  //
  // Maximum contiguous buffer block size is dynamically determined by a GFlag.
  // To be safe, you should never reserve more than 1K bytes.
  //
  // This method is provided for advance users.
  //
  // Pointer to the beginning of reserved block is returned.
  char* Reserve(std::size_t bytes) {
    static const auto kMaxBytes = 1024;

    FLARE_CHECK_LE(bytes, kMaxBytes,
                   "At most [{}] bytes may be reserved in a single call.",
                   kMaxBytes);
    if (SizeAvailable() < bytes) {  // No enough contiguous buffer space,
                                    // allocated a new one then.
      FlushCurrentBlock();
      InitializeNextBlock();
    }
    auto ptr = data();
    MarkWritten(bytes);
    return ptr;
  }

  // Total number of bytes written.
  std::size_t ByteSize() const noexcept { return nb_.ByteSize() + used_; }

  // Clean up internal state and move buffer built out.
  //
  // CAUTION: You may not touch the builder after calling this method.
  NoncontiguousBuffer DestructiveGet() noexcept {
    FlushCurrentBlock();
    return std::move(nb_);
  }

  // Some helper methods for building buffer more conveniently go below.

  // Append `length` bytes from `ptr` into its internal buffer.
  void Append(const void* ptr, std::size_t length) {
    // We speculatively increase `used_` here. This may cause it to temporarily
    // overflow. In that unlikely case, we failback to revert the change and
    // call `AppendSlow` instead.
    auto current = data();
    used_ += length;
    // If `used_` equals to buffer block size, we CANNOT use the optimization,
    // as a new block need to be allocated (which is done by `AppendSlow()`).
    if (FLARE_LIKELY(used_ < current_->size())) {
      // GCC 10 reports a false positive here.
#if __GNUC__ == 10
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
      memcpy(current, ptr, length);
#if __GNUC__ == 10
#pragma GCC diagnostic pop
#endif
      return;
    }
    used_ -= length;  // Well we failed..
    AppendSlow(ptr, length);
  }

  // Append `s` to its internal buffer.
  void Append(std::string_view s) { return Append(s.data(), s.size()); }

  void Append(PolymorphicBuffer buffer) {
    // If the `buffer` is small enough, and append it to the current block does
    // not introduce new block, copying it can help in reducing internal
    // fragmentation.
    if (buffer.size() < kAppendViaCopyThreshold &&
        current_->size() - used_ >= buffer.size()) {
      Append(buffer.data(), buffer.size());
      return;
    }
    // If there's nothing in our internal buffer, we don't need to flush it.
    if (used_) {
      FlushCurrentBlock();
      InitializeNextBlock();
    }
    nb_.Append(std::move(buffer));
  }

  void Append(NoncontiguousBuffer buffer) {
    if (buffer.ByteSize() < kAppendViaCopyThreshold &&
        current_->size() - used_ >= buffer.ByteSize()) {
      AppendCopy(buffer);
      return;
    }
    if (used_) {
      FlushCurrentBlock();
      InitializeNextBlock();
    }
    nb_.Append(std::move(buffer));
  }

  // Append char `c` to its internal buffer.
  void Append(char c) { Append(static_cast<unsigned char>(c)); }
  void Append(unsigned char c) {
    FLARE_DCHECK(SizeAvailable());  // We never left internal buffer full.
    *reinterpret_cast<unsigned char*>(data()) = c;
    MarkWritten(1);
  }

  // If you want to append several **small** buffers that are **unlikely** to
  // cause new buffer block to be allocated, this method saves some arithmetic
  // operations.
  //
  // `Ts::data()` & `Ts::size()` must be available for the type to be used with
  // this method.
  template <class... Ts,
            class = std::void_t<decltype(detail::data(std::declval<Ts&>()))...>,
            class = std::void_t<decltype(detail::size(std::declval<Ts&>()))...>,
            class = std::enable_if_t<(sizeof...(Ts) > 1)>>
  void Append(const Ts&... buffers) {
    auto current = data();
    auto total = (detail::size(buffers) + ...);
    used_ += total;
    if (FLARE_LIKELY(used_ < current_->size())) {
      UncheckedAppend(current, buffers...);
      return;
    }

    used_ -= total;
    // Initializers are evaluated in order, so order of the buffers is kept.
    [[maybe_unused]] int dummy[] = {(Append(buffers), 0)...};
  }

  [[deprecated(
      "Integral types other than `(unsigned) char` cannot be appended to "
      "`NoncontiguousBuffer` directly. You need to be explicit about byte "
      "order when dealing with numbers. Therefore, you should encode your "
      "number to byte stream and add it via overload `Append(const char* ptr, "
      "std::size_t length)` instead.")]] void
  Append(int);  // Not defined on purpose.

 private:
  // Allocate a new buffer.
  void InitializeNextBlock();

  // Move the buffer block we're working on into the non-contiguous buffer we're
  // building.
  void FlushCurrentBlock();

  // Slow case for `Append`.
  void AppendSlow(const void* ptr, std::size_t length);

  // Copy `buffer` into us. This is an optimization for small `buffer`s, so as
  // to reduce internal fragmentation.
  void AppendCopy(const NoncontiguousBuffer& buffer);

  template <class T, class... Ts>
  [[gnu::always_inline]] void UncheckedAppend(char* ptr, const T& sv) {
    memcpy(ptr, detail::data(sv), detail::size(sv));
  }

  template <class T, class... Ts>
  [[gnu::always_inline]] void UncheckedAppend(char* ptr, const T& sv,
                                              const Ts&... svs) {
    memcpy(ptr, detail::data(sv), detail::size(sv));
    UncheckedAppend(ptr + detail::size(sv), svs...);
  }

 private:
  NoncontiguousBuffer nb_;
  std::size_t used_;
  RefPtr<NativeBufferBlock> current_;
};

// Helper functions go below.

namespace detail {

// ... Yeah, really slow.
void FlattenToSlowSlow(const NoncontiguousBuffer& nb, void* buffer,
                       std::size_t size);

}  // namespace detail

NoncontiguousBuffer CreateBufferSlow(std::string_view s);
NoncontiguousBuffer CreateBufferSlow(const void* ptr, std::size_t size);

std::string FlattenSlow(
    const NoncontiguousBuffer& nb,
    std::size_t max_bytes = std::numeric_limits<std::size_t>::max());

// `delim` is included in the result string.
std::string FlattenSlowUntil(
    const NoncontiguousBuffer& nb, std::string_view delim,
    std::size_t max_bytes = std::numeric_limits<std::size_t>::max());

// Caller is responsible for ensuring `nb.ByteSize()` is no less than `size`.
inline void FlattenToSlow(const NoncontiguousBuffer& nb, void* buffer,
                          std::size_t size) {
  if (FLARE_LIKELY(size <= nb.FirstContiguous().size())) {
    memcpy(buffer, nb.FirstContiguous().data(), size);
  }
  return detail::FlattenToSlowSlow(nb, buffer, size);
}

// Make a buffer block that references to memory region pointed to by `buffer`.
//
// It's your responsibility to make sure memory regions referenced are valid and
// not mutated until the resulting buffer is consumed (i.e., destroyed).
PolymorphicBuffer MakeReferencingBuffer(const void* ptr, std::size_t size);

// This overload accepts a completion callback. This provide a way for the
// creator to be notified when the framework finished using the buffer.
template <class F>
PolymorphicBuffer MakeReferencingBuffer(const void* ptr, std::size_t size,
                                        F&& completion_cb) {
  using BufferBlock = ReferencingBufferBlock<std::remove_reference_t<F>>;
  return PolymorphicBuffer(
      MakeRefCounted<BufferBlock>(ptr, size, std::forward<F>(completion_cb)), 0,
      size);
}

// Create a buffer that owns the container passed to this method.
PolymorphicBuffer MakeForeignBuffer(std::string buffer);

// Create a buffer that owns `buffer`.
//
// `T` may only be one of the following types:
//
// - `std::byte`.
// - Builtin integral types.
// - Builtin floating-point types.
template <class T>
PolymorphicBuffer MakeForeignBuffer(std::vector<T> buffer);

}  // namespace flare

#endif  // FLARE_BASE_BUFFER_H_
