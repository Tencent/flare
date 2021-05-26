// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_BASE_BUFFER_VIEW_H_
#define FLARE_BASE_BUFFER_VIEW_H_

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "flare/base/buffer.h"
#include "flare/base/internal/early_init.h"

namespace flare {

// This class provides a visually contiguous byte-wise view of a buffer.
//
// Performance note: Due to implementation details, scanning through a buffer
// via this class is much slower than scanning the buffer in a non-contiguous
// fashion.
//
// A `ForwardIterator` is returned. Random access is not supported.
class NoncontiguousBufferForwardView {
 public:
  class const_iterator;
  using iterator = const_iterator;

  NoncontiguousBufferForwardView() = default;
  explicit NoncontiguousBufferForwardView(const NoncontiguousBuffer& buffer)
      : buffer_(&buffer) {}

  const_iterator begin() const noexcept;
  const_iterator end() const noexcept;

  bool empty() const noexcept { return buffer_->Empty(); }
  std::size_t size() const noexcept { return buffer_->ByteSize(); }

 private:
  const NoncontiguousBuffer* buffer_ =
      &internal::EarlyInitConstant<NoncontiguousBuffer>();
};

// This class provides random access into a buffer.
//
// Internally this class build a mapping of all discontiguous buffer blocks.
// This comes at a cost. Therefore, unless you absolutely need it, stick with
// "forward" view.
//
// Same performance consideration as of `NoncontiguousBufferForwardView` also
// applies here.
class NoncontiguousBufferRandomView {
 public:
  class const_iterator;
  using iterator = const_iterator;

  NoncontiguousBufferRandomView();
  explicit NoncontiguousBufferRandomView(const NoncontiguousBuffer& buffer);

  // Random access. This is slower than traversal.
  char operator[](std::size_t offset) const noexcept {
    FLARE_DCHECK_LT(offset, size());
    auto&& [off, iter] = FindSegmentMustSucceed(offset);
    return iter->data()[offset - off];
  }

  const_iterator begin() const noexcept;
  const_iterator end() const noexcept;

  bool empty() const noexcept { return byte_size_ == 0; }
  std::size_t size() const noexcept { return byte_size_; }

 private:
  std::pair<std::size_t, NoncontiguousBuffer::const_iterator>
  FindSegmentMustSucceed(std::size_t offset) const noexcept;

 private:
  std::size_t byte_size_ = 0;
  // [Starting offset -> iterator].
  std::vector<std::pair<std::size_t, NoncontiguousBuffer::const_iterator>>
      offsets_;
};

///////////////////////////////////////
// Implementation goes below.        //
///////////////////////////////////////

class NoncontiguousBufferForwardView::const_iterator {
 public:
  using difference_type = std::ptrdiff_t;
  using value_type = char;
  using pointer = const char*;
  using reference = const char&;
  using iterator_category = std::forward_iterator_tag;

  const_iterator() = default;  // `end()`.

  char operator*() const noexcept {
    FLARE_DCHECK(current_ != end_, "Dereferencing an invalid iterator.");
    FLARE_DCHECK_LT(byte_offset_, current_->size());
    return *(current_->data() + byte_offset_);
  }

  const_iterator& operator++() noexcept {
    FLARE_DCHECK(current_ != end_);
    FLARE_DCHECK(byte_offset_ < current_->size());
    ++byte_offset_;
    if (FLARE_UNLIKELY(byte_offset_ == current_->size())) {
      byte_offset_ = 0;
      ++current_;
    }
    return *this;
  }

  bool operator==(const const_iterator& iter) const noexcept {
    return current_ == iter.current_ && byte_offset_ == iter.byte_offset_;
  }
  bool operator!=(const const_iterator& iter) const noexcept {
    return current_ != iter.current_ || byte_offset_ != iter.byte_offset_;
  }

 private:
  friend class NoncontiguousBufferForwardView;

  const_iterator(NoncontiguousBuffer::const_iterator begin,
                 NoncontiguousBuffer::const_iterator end)
      : current_(begin), end_(end), byte_offset_(0) {}

 private:
  NoncontiguousBuffer::const_iterator current_, end_;
  std::size_t byte_offset_;
};

inline NoncontiguousBufferForwardView::const_iterator
NoncontiguousBufferForwardView::begin() const noexcept {
  return const_iterator(buffer_->begin(), buffer_->end());
}

inline NoncontiguousBufferForwardView::const_iterator
NoncontiguousBufferForwardView::end() const noexcept {
  return const_iterator();
}

class NoncontiguousBufferRandomView::const_iterator {
 public:
  using difference_type = std::ptrdiff_t;
  using value_type = char;
  using pointer = const char*;
  using reference = const char&;
  using iterator_category = std::forward_iterator_tag;

  const_iterator() = default;  // `end()`.

  char operator*() const noexcept {
    FLARE_DCHECK(current_ != end_, "Dereferencing an invalid iterator.");
    FLARE_DCHECK_LT(seg_offset_, current_->size());
    return *(current_->data() + seg_offset_);
  }

  const_iterator& operator+=(std::ptrdiff_t offset) noexcept {
    SeekTo(offset);
    return *this;
  }

  const_iterator operator+(std::ptrdiff_t offset) noexcept {
    auto copy(*this);
    copy += offset;
    return copy;
  }

  std::ptrdiff_t operator-(const const_iterator& other) const noexcept {
    return byte_offset_ - other.byte_offset_;
  }

  const_iterator& operator++() noexcept {
    FLARE_DCHECK(current_ != end_);
    FLARE_DCHECK(seg_offset_ < current_->size());
    ++seg_offset_;
    if (FLARE_UNLIKELY(seg_offset_ == current_->size())) {
      seg_offset_ = 0;
      ++current_;
    }
    ++byte_offset_;
    return *this;
  }

  // TODO(luobogao): Other operators.

  bool operator==(const const_iterator& iter) const noexcept {
    FLARE_CHECK_EQ(view_, iter.view_);
    return byte_offset_ == iter.byte_offset_;
  }
  bool operator!=(const const_iterator& iter) const noexcept {
    FLARE_CHECK_EQ(view_, iter.view_);
    return byte_offset_ != iter.byte_offset_;
  }

 private:
  friend class NoncontiguousBufferRandomView;

  explicit const_iterator(const NoncontiguousBufferRandomView* view)
      : view_(view) {
    byte_offset_ = seg_offset_ = 0;
    current_ = view->offsets_.front().second;
    end_ = view->offsets_.back().second;
  }

  void SeekTo(std::size_t offset) noexcept {
    FLARE_CHECK_LE(offset, view_->size());
    auto&& [off, iter] = view_->FindSegmentMustSucceed(offset);
    byte_offset_ = offset;
    seg_offset_ = offset - off;
    current_ = iter;
  }

 private:
  const NoncontiguousBufferRandomView* view_;
  std::size_t byte_offset_;
  NoncontiguousBuffer::const_iterator current_, end_;
  std::size_t seg_offset_;
};

inline NoncontiguousBufferRandomView::const_iterator
NoncontiguousBufferRandomView::begin() const noexcept {
  return const_iterator(this);
}

inline NoncontiguousBufferRandomView::const_iterator
NoncontiguousBufferRandomView::end() const noexcept {
  const_iterator result(this);
  result.SeekTo(size());
  return result;
}

}  // namespace flare

#endif  // FLARE_BASE_BUFFER_VIEW_H_
