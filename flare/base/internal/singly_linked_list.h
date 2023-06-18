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

#ifndef FLARE_BASE_INTERNAL_SINGLY_LINKED_LIST_H_
#define FLARE_BASE_INTERNAL_SINGLY_LINKED_LIST_H_

#include <cstddef>
#include <utility>

#include "flare/base/internal/logging.h"

namespace flare::internal {

struct SinglyLinkedListEntry {
  SinglyLinkedListEntry* next = this;
};

// You saw `DoublyLinkedList`, now you see `SinglyLinkedList`-one.
//
// For **really** performance sensitive path, this one can be faster than its
// doubly-linked counterpart.
//
// For internal use only. DO NOT USE IT.
//
// TODO(luobogao): The interface does not quite match STL's, we might want to
// align with them.
template <class T, SinglyLinkedListEntry T::*kEntry>
class SinglyLinkedList {
 public:
  class iterator;
  class const_iterator;

  // Initialize an empty list.
  constexpr SinglyLinkedList();

  // Access first element in the list.
  //
  // Precondition: `!empty()` holds.
  constexpr T& front() noexcept;
  constexpr const T& front() const noexcept;

  // Access last element in the list.
  //
  // Precondition: `!empty()` holds.
  constexpr T& back() noexcept;
  constexpr const T& back() const noexcept;

  // Pop the first element (at head) in the list, or `nullptr` if the list is
  // empty.
  constexpr T* pop_front() noexcept;

  // `pop_back` cannot be implemented efficiently on singly-linked list.

  // Insert an element at list's head.
  constexpr void push_front(T* entry) noexcept;

  // Push an element into the list. The element is inserted at the tail of the
  // list.
  constexpr void push_back(T* entry) noexcept;

  // `erase` is not implemented due to the difficulty in implementation.

  // Move all elements from `from` to the tail of this list.
  //
  // TODO(luobogao): Support `splice(pos, from)`.
  constexpr void splice(SinglyLinkedList&& from) noexcept;

  // Swap two lists.
  constexpr void swap(SinglyLinkedList& other) noexcept;

  // Get size of the list.
  constexpr std::size_t size() const noexcept;

  // Test if the list is empty.
  constexpr bool empty() const noexcept;

  // Iterate through the list.
  constexpr iterator begin() noexcept;
  constexpr iterator end() noexcept;
  constexpr const_iterator begin() const noexcept;
  constexpr const_iterator end() const noexcept;
  // TODO(luobogao): `cbegin()` / `cend()`.

  // Movable.
  //
  // Moving into non-empty list is UNDEFINED.
  SinglyLinkedList(SinglyLinkedList&& other) noexcept;
  SinglyLinkedList& operator=(
      SinglyLinkedList&& other) noexcept;  // CAUTION ABOUT LEAK!

  // Non-copyable
  SinglyLinkedList(const SinglyLinkedList&) = delete;
  SinglyLinkedList& operator=(const SinglyLinkedList&) = delete;

 private:
  // `offsetof(T, kEntry)`.
  inline static const std::uintptr_t kSinglyLinkedListEntryOffset =
      reinterpret_cast<std::uintptr_t>(&(reinterpret_cast<T*>(0)->*kEntry));

  // Cast from `T*` to pointer to its `SinglyLinkedListEntry` field.
  static constexpr SinglyLinkedListEntry* node_cast(T* ptr) noexcept;
  static constexpr const SinglyLinkedListEntry* node_cast(
      const T* ptr) noexcept;

  // Cast back from what's returned by `node_cast` to original `T*`.
  static constexpr T* object_cast(SinglyLinkedListEntry* entry) noexcept;
  static constexpr const T* object_cast(
      const SinglyLinkedListEntry* entry) noexcept;

 private:
  std::size_t size_{};
  SinglyLinkedListEntry* next_{};
  SinglyLinkedListEntry* tail_{};  // For implementing `push_back` efficiently.
};

// Mutable iterator.
template <class T, SinglyLinkedListEntry T::*kEntry>
class SinglyLinkedList<T, kEntry>::iterator {
 public:
  constexpr iterator() : current_(nullptr) {}
  constexpr iterator& operator++() noexcept {
    current_ = current_->next;
    return *this;
  }
  constexpr T* operator->() const noexcept { return object_cast(current_); }
  constexpr T& operator*() const noexcept { return *object_cast(current_); }
  constexpr bool operator!=(const iterator& iter) const noexcept {
    return current_ != iter.current_;
  }

  // TODO(luobogao): Post-increment / operator ==.
 private:
  friend class SinglyLinkedList<T, kEntry>;
  constexpr explicit iterator(SinglyLinkedListEntry* start) noexcept
      : current_(start) {}

 private:
  SinglyLinkedListEntry* current_;
};

// Const iterator.
template <class T, SinglyLinkedListEntry T::*kEntry>
class SinglyLinkedList<T, kEntry>::const_iterator {
 public:
  constexpr const_iterator() : current_(nullptr) {}
  constexpr /* implicit */ const_iterator(const iterator& from)
      : current_(from.current_) {}
  constexpr const_iterator& operator++() noexcept {
    current_ = current_->next;
    return *this;
  }
  constexpr const T* operator->() const noexcept {
    return object_cast(current_);
  }
  constexpr const T& operator*() const noexcept {
    return *object_cast(current_);
  }
  constexpr bool operator!=(const const_iterator& iter) const noexcept {
    return current_ != iter.current_;
  }
  constexpr bool operator==(const const_iterator& iter) const noexcept {
    return current_ == iter.current_;
  }

  // TODO(luobogao): Post-increment / operator ==.
 private:
  friend class SinglyLinkedList<T, kEntry>;
  constexpr explicit const_iterator(const SinglyLinkedListEntry* start) noexcept
      : current_(start) {}

 private:
  const SinglyLinkedListEntry* current_;
};

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr inline SinglyLinkedList<T, kEntry>::SinglyLinkedList() = default;

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr T& SinglyLinkedList<T, kEntry>::front() noexcept {
  FLARE_DCHECK(!empty(), "Calling `front()` on empty list is undefined.");
  return *object_cast(next_);
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr const T& SinglyLinkedList<T, kEntry>::front() const noexcept {
  return const_cast<SinglyLinkedList*>(this)->front();
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr T& SinglyLinkedList<T, kEntry>::back() noexcept {
  FLARE_DCHECK(!empty(), "Calling `back()` on empty list is undefined.");
  return *object_cast(tail_);
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr const T& SinglyLinkedList<T, kEntry>::back() const noexcept {
  return const_cast<SinglyLinkedList*>(this)->back();
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr T* SinglyLinkedList<T, kEntry>::pop_front() noexcept {
  FLARE_DCHECK(!empty(), "Calling `pop_front()` on empty list is undefined.");
  auto rc = next_;
  next_ = next_->next;
  if (!--size_) {
    tail_ = nullptr;
    FLARE_DCHECK(!next_);
  }
  return object_cast(rc);
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr void SinglyLinkedList<T, kEntry>::push_front(T* entry) noexcept {
  auto ptr = node_cast(entry);
  ptr->next = next_;
  next_ = ptr;
  if (!size_++) {
    tail_ = next_;
  }
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr void SinglyLinkedList<T, kEntry>::push_back(T* entry) noexcept {
  auto ptr = node_cast(entry);
  ptr->next = nullptr;
  if (size_++) {
    tail_ = tail_->next = ptr;
  } else {
    next_ = tail_ = ptr;
  }
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr void SinglyLinkedList<T, kEntry>::splice(
    SinglyLinkedList&& from) noexcept {
  if (empty()) {
    swap(from);
    return;
  }
  if (from.empty()) {
    return;
  }
  tail_->next = from.next_;
  tail_ = from.tail_;
  size_ += from.size_;
  from.next_ = from.tail_ = nullptr;
  from.size_ = 0;
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr void SinglyLinkedList<T, kEntry>::swap(
    SinglyLinkedList& other) noexcept {
  std::swap(next_, other.next_);
  std::swap(tail_, other.tail_);
  std::swap(size_, other.size_);
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr std::size_t SinglyLinkedList<T, kEntry>::size() const noexcept {
  return size_;
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr bool SinglyLinkedList<T, kEntry>::empty() const noexcept {
  FLARE_DCHECK(!size_ == !next_);
  return !size_;
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr typename SinglyLinkedList<T, kEntry>::iterator
SinglyLinkedList<T, kEntry>::begin() noexcept {
  return iterator(next_);
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr typename SinglyLinkedList<T, kEntry>::iterator
SinglyLinkedList<T, kEntry>::end() noexcept {
  return iterator(nullptr);
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr typename SinglyLinkedList<T, kEntry>::const_iterator
SinglyLinkedList<T, kEntry>::begin() const noexcept {
  return const_iterator(next_);
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr typename SinglyLinkedList<T, kEntry>::const_iterator
SinglyLinkedList<T, kEntry>::end() const noexcept {
  return const_iterator(nullptr);
}

template <class T, SinglyLinkedListEntry T::*kEntry>
SinglyLinkedList<T, kEntry>::SinglyLinkedList(
    SinglyLinkedList&& other) noexcept {
  swap(other);
}

template <class T, SinglyLinkedListEntry T::*kEntry>
SinglyLinkedList<T, kEntry>& SinglyLinkedList<T, kEntry>::operator=(
    SinglyLinkedList&& other) noexcept {
  FLARE_CHECK(empty(), "Moving into non-empty list will likely leak.");
  swap(other);
  return *this;
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr SinglyLinkedListEntry* SinglyLinkedList<T, kEntry>::node_cast(
    T* ptr) noexcept {
  return &(ptr->*kEntry);
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr const SinglyLinkedListEntry* SinglyLinkedList<T, kEntry>::node_cast(
    const T* ptr) noexcept {
  return &(ptr->*kEntry);
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr T* SinglyLinkedList<T, kEntry>::object_cast(
    SinglyLinkedListEntry* entry) noexcept {
  return reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(entry) -
                              kSinglyLinkedListEntryOffset);
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr const T* SinglyLinkedList<T, kEntry>::object_cast(
    const SinglyLinkedListEntry* entry) noexcept {
  return reinterpret_cast<const T*>(reinterpret_cast<std::uintptr_t>(entry) -
                                    kSinglyLinkedListEntryOffset);
}

template <class T, SinglyLinkedListEntry T::*kEntry>
constexpr void swap(SinglyLinkedList<T, kEntry>& left,
                    SinglyLinkedList<T, kEntry>& right) noexcept {
  left.swap(right);
}

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_SINGLY_LINKED_LIST_H_
