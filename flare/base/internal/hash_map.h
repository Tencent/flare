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

#ifndef FLARE_BASE_INTERNAL_HASH_MAP_H_
#define FLARE_BASE_INTERNAL_HASH_MAP_H_

#include <chrono>
#include <functional>
#include <iterator>
#include <memory>
#include <tuple>
#include <utility>

#include "flare/base/internal/logging.h"
#include "flare/base/internal/meta.h"
#include "flare/base/likely.h"

namespace flare::internal {

// Primary template, not defined intentionally.
//
// Adopted from `gdt::Hasher`. @sa: `common/hash/hash.h`.
template <class T = void, class = void>
struct Hash;

// Transparent specialization.
template <>
struct Hash<void>;

// Yet another map. This implementation is really.. not that fast. (Hopefully
// it's fast *enough*, though.)
//
// THIS MAP IS OPTIMIZED FOR FAST LOOKUP **ONLY**. MUTATION DOES NOT PERFORM
// EQUALLY WELL (EVEN WHEN COMPARED WITH STD'S COUNTERPART.).
//
// DIFFERENT FROM HOW `STD`-COUNTERPART BEHAVES, FOR THIS MAP, ALL ITERATORS ARE
// INVALIDATED ON UPDATE. THEREFORE, DON'T BLINDLY USE THIS ONE AS A DROP-IN
// REPLACEMENT.
//
// Inspired by `butil::FlatMap`, credits to them. Please note that albeit its
// name in brpc, it's a hash map instead of what's generally thought as a
// `flat_map` (@sa: p0429r7, boost.flat_map).
//
// @sa: https://github.com/apache/incubator-brpc/blob/master/docs/cn/flatmap.md
template <class K, class V, class H = Hash<>, class E = std::equal_to<>>
class HashMap {
  using value_type = std::pair<const K, V>;  // Not exposed publicly.

 public:
  class iterator;
  class const_iterator;

  // `initial_capacity` may not be less than 4.
  HashMap() : HashMap(16) {}
  explicit HashMap(std::size_t initial_capacity);
  HashMap(HashMap&& other) noexcept;
  HashMap& operator=(HashMap&&) noexcept;
  HashMap(const HashMap& other);
  HashMap& operator=(const HashMap& other);
  ~HashMap();

  /* implicit */ HashMap(std::initializer_list<value_type> init);

  iterator begin() noexcept;
  const_iterator begin() const noexcept;
  iterator end() noexcept;
  const_iterator end() const noexcept;
  // I don't think `cbegin()` / `cend()` make much sense.

  bool empty() const noexcept { return size() == 0; }
  std::size_t size() const noexcept { return size_; }

  void clear() noexcept;
  template <class T, class U>
  iterator insert_or_assign(T&& key, U&& value);
  template <class T,
            class = std::enable_if_t<!std::is_convertible_v<T, const_iterator>>>
  std::size_t erase(const T& key) noexcept;
  iterator erase(const_iterator pos) noexcept;
  void swap(HashMap& other) noexcept;

  // We don't use exceptions, so `HashMap::at` crashes on lookup failure.
  template <class T>
  V& at(const T& key) noexcept;
  template <class T>
  const V& at(const T& key) const noexcept;
  template <class T>
  V& operator[](T&& key);
  template <class T>
  iterator find(const T& key) noexcept;
  template <class T>
  const_iterator find(const T& key) const noexcept;
  template <class T>
  bool contains(const T& key) const noexcept;

  ////////////////////////////////////////////////////////////////////////
  // Our extensions go below. (Primarily for performance purpose.)      //
  ////////////////////////////////////////////////////////////////////////

  // If you expect the key to be present, and being no hash collision should
  // appear, this one is annotated with that in mind, and could perform better
  // than `find`.
  template <class T>
  V* TryGet(const T& key) noexcept;
  template <class T>
  const V* TryGet(const T& key) const noexcept;

 private:
  // If possible, we want to save the compiler a multiplication (by making size
  // of `Node` a power of two, since we'll be indexing into it frequently.)
  template <class T>
  struct storage_size;
  template <class T>
  inline static constexpr std::size_t storage_size_v = storage_size<T>::value;

  struct Node;
  struct Cursor;
  using Location = std::pair<std::size_t, Node*>;

  void InitializeFor(std::size_t capacity);
  template <class T>
  std::size_t GetBucket(const T& key) const;

  void Rehash();
  void RehashIfNecessary();

  void MergeFrom(const HashMap& other);

  Cursor FirstNonEmptyCursor() const noexcept;
  void NextCursor(Cursor* cursor, bool initialized) const noexcept;
  Cursor SentinelCursor() const noexcept;
  template <class T>
  Location LocateKey(const T& key) const noexcept;
  template <class T>
  Location LocateKeySlow(const T& key, std::size_t idx,
                         Node* current) const noexcept;
  template <class T>
  Location CreateOrLocateKey(const T& key);
  template <class T>
  Location CreateOrLocateKeySlow(const T& key, std::size_t idx, Node* current);

  template <class T>
  V* TryGetSlow(Node* bucket, const T& key) noexcept;

  static constexpr std::size_t NextPowerOfTwo(std::size_t v);

 private:
  std::unique_ptr<Node[]> nodes_;
  std::size_t capacity_;
  std::size_t mask_;
  std::size_t rehash_threshold_;
  std::size_t size_;
};

////////////////////////////////////////
// Implementation goes below.         //
////////////////////////////////////////

namespace detail::hash_map {

// Evaluates to `true` if specialized (defined) for `std::hash<...>`, `false`
// otherwise.
template <class T, class = void>
struct is_std_hashable : std::false_type {};
template <class T>
struct is_std_hashable<T, std::void_t<decltype(std::hash<T>{})>>
    : std::true_type {};
template <class T>
constexpr auto is_std_hashable_v = is_std_hashable<T>::value;

static_assert(is_std_hashable_v<int>,
              "Unexpected: `int` is not hashable by `std::hash<...>`.");
static_assert(!is_std_hashable_v<void>,
              "Unexpected: `void` is hashable by `std::hash<...>`.");

// Evaluates to `true` if `std::begin(T{})` is well-formed, `false` otherwise.
template <class T, class = void>
struct is_iterable : std::false_type {};
template <class T>
struct is_iterable<T, std::void_t<decltype(std::begin(std::declval<T&>()))>>
    : std::true_type {};
template <class T>
constexpr auto is_iterable_v = is_iterable<T>::value;

static_assert(is_iterable_v<int[5]>, "Unexpected: `int [5]` is not iterable.");
static_assert(!is_iterable_v<int>, "Unexpected: `int` is iterable.");

// Evaluates to `true` if `T` is a `char` array, `false` otherwise.
template <class T, class = void>
struct is_char_array : std::false_type {};
template <std::size_t N>
struct is_char_array<char[N]> : std::true_type {};
// `const char [N]` is not specialized. We're relying on our caller to remove
// cvr qualifiers.
template <class T>
constexpr auto is_char_array_v = is_char_array<T>::value;

static_assert(is_char_array_v<char[5]>,
              "Unexpected: `char [5]` is no a char array.");
static_assert(!is_char_array_v<char*>, "Unexpected: `char *` is a char array.");

// Helper alias for indexing.
template <std::size_t I>
using index_t = std::integral_constant<std::size_t, I>;

// Shamelessly copied from boost 1.68. Kept in header for inlining.
//
// @sa: {boost 1.68}/container_hash/hash.hpp
inline void HashCombine(std::size_t* seed, std::size_t value) {
  *seed ^= value + 0x9e3779b9 + (*seed << 6) + (*seed >> 2);
}

// Hash a range of values.
template <class I>
inline std::size_t HashRange(I begin, I end) {
  using T = decltype(*std::declval<I>());

  Hash<remove_cvref_t<T>> hasher;
  std::size_t seed = 0;

  for (I iter = begin; iter != end; ++iter) {
    HashCombine(&seed, hasher(*iter));
  }

  return seed;
}

}  // namespace detail::hash_map

// Transparent version.
template <>
struct Hash<void> {
  template <class T>
  std::size_t operator()(T&& val) const noexcept {
    return Hash<remove_cvref_t<T>>()(std::forward<T>(val));
  }
};

// For those whose `std::hash<...>` is specialized (defined), we use them.
//
// Pointer types are not delegated to `std::hash`, libstdc++'s QoI is not so
// good here. (See below.)
template <class T>
struct Hash<T, std::enable_if_t<detail::hash_map::is_std_hashable_v<T> &&
                                !std::is_pointer_v<T>>> : std::hash<T> {};

// The C-style strings.
//
// We cannot rely on the specialization of array (container) types as
// they'll recursively rely on `Hasher<T>` [T = `char`], which is not defined.
//
// COMMENTED OUT INTENTIONALLY, I'M NOT SURE USING `const char*` AS THE KEY IS
// APPROPRIATE. BESIDES, USING `std::hash<std::string_view>` FOR `const char*`
// INCURS AN UNNECESSARY CALL TO `strlen`, WHICH HURTS PERFORMANCE.
//
// template <class T>
// struct Hash<T, std::enable_if_t<!detail::hash_map::is_std_hashable_v<T> &&
//                                 detail::hash_map::is_char_array_v<T>>>
//     : Hash<std::string_view> {};

// For container types (but not those already specialized `std::hash<...>`
// -- `std::string`, for example).
//
// Arrays are also handled here (except for `char []`, see above).
template <class T>
struct Hash<T, std::enable_if_t<!detail::hash_map::is_std_hashable_v<T> &&
                                !detail::hash_map::is_char_array_v<T> &&
                                detail::hash_map::is_iterable_v<T>>> {
  std::size_t operator()(const T& container) const {
    return detail::hash_map::HashRange(std::begin(container),
                                       std::end(container));
  }
};

// For pointer types.
template <class T>
struct Hash<T*> {
  std::size_t operator()(const T* ptr) const noexcept {
    // We should take alignment into consideration.
    if constexpr (std::is_void_v<T>) {
      return reinterpret_cast<std::uintptr_t>(ptr) / 8;
    } else {
      return reinterpret_cast<std::uintptr_t>(ptr) / alignof(T);
    }
  }
};

// Specialized version for `std::pair`.
template <class T, class U>
struct Hash<std::pair<T, U>> {
  static_assert(!detail::hash_map::is_std_hashable_v<std::pair<T, U>>,
                "Unexpected: `std::pair<T, U>` is already specialized for "
                "`std::hash<...>`.");

  std::size_t operator()(const std::pair<T, U>& pair) const {
    std::size_t seed = 0;
    detail::hash_map::HashCombine(&seed, Hash<T>{}(pair.first));
    detail::hash_map::HashCombine(&seed, Hash<U>{}(pair.second));
    return seed;
  }
};

// Specialized version for `std::tuple`.
template <class... Ts>
struct Hash<std::tuple<Ts...>> {
  static_assert(!detail::hash_map::is_std_hashable_v<std::tuple<Ts...>>,
                "Unexpected: `std::tuple<Ts...>` is already specialized for "
                "`std::hash<...>`.");

  std::size_t operator()(const std::tuple<Ts...>& tuple) const {
    std::size_t seed = 0;
    CombineHashes(&seed, tuple, detail::hash_map::index_t<0>{});
    return seed;
  }

 private:
  // For better readability, we alias the hashers for the elements.
  template <std::size_t I>
  using ElementHasher =
      Hash<remove_cvref_t<std::tuple_element_t<I, std::tuple<Ts...>>>>;

  // Recursion boundary, when there's no element left.
  static void CombineHashes(std::size_t*, const std::tuple<Ts...>&,
                            detail::hash_map::index_t<sizeof...(Ts)>) {
  }  // NOTHING.

  // Helper method. Combines hashes of each element.
  template <std::size_t I>
  static void CombineHashes(std::size_t* seed, const std::tuple<Ts...>& tuple,
                            detail::hash_map::index_t<I>) {
    detail::hash_map::HashCombine(seed, ElementHasher<I>{}(std::get<I>(tuple)));
    CombineHashes(seed, tuple, detail::hash_map::index_t<I + 1>{});
  }
};

// Specialized version for `std::chrono::duration<...>`
template <class Rep, class Period>
struct Hash<std::chrono::duration<Rep, Period>> {
  static_assert(
      !detail::hash_map::is_std_hashable_v<std::chrono::duration<Rep, Period>>,
      "Unexpected: `std::chrono::duration<...>` is already specialized for "
      "`std::hash<...>`.");
  std::size_t operator()(
      const std::chrono::duration<Rep, Period>& duration) const noexcept {
    auto count = duration.count();
    return Hash<decltype(count)>{}(count);
  }
};

template <class K, class V, class H, class E>
struct HashMap<K, V, H, E>::Cursor {
  const HashMap* self;
  std::size_t bucket_id;
  Node* current;
  constexpr bool operator==(const Cursor& other) const noexcept {
    return current == other.current;  // Comparing node pointer is sufficient.
  }
};

template <class K, class V, class H, class E>
class HashMap<K, V, H, E>::iterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = std::pair<const K, V>;
  using pointer = value_type*;
  using reference = value_type&;
  using difference_type = std::ptrdiff_t;

  constexpr iterator() = default;
  constexpr iterator& operator++() noexcept {
    cursor_.self->NextCursor(&cursor_, true);
    return *this;
  }
  constexpr iterator operator++(int) noexcept {
    auto old = *this;
    ++*this;
    return old;
  }

  constexpr reference operator*() const noexcept {
    return *cursor_.current->GetKV();
  }
  constexpr pointer operator->() const noexcept {
    return cursor_.current->GetKV();
  }
  constexpr bool operator==(const iterator& other) const noexcept {
    return cursor_ == other.cursor_;
  }
  constexpr bool operator!=(const iterator& other) const noexcept {
    return !(cursor_ == other.cursor_);
  }

 private:
  friend class HashMap<K, V, H, E>;

  constexpr explicit iterator(Cursor cursor) : cursor_(cursor) {}
  constexpr const Cursor& GetCursor() const { return cursor_; }

 private:
  Cursor cursor_;
};

template <class K, class V, class H, class E>
class HashMap<K, V, H, E>::const_iterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = std::pair<const K, V>;
  using pointer = value_type*;
  using reference = value_type&;
  using difference_type = std::ptrdiff_t;

  constexpr const_iterator() = default;
  constexpr /* implicit */ const_iterator(const iterator& from)
      : cursor_(from.cursor_) {}
  constexpr const_iterator& operator++() noexcept {
    cursor_.self->NextCursor(&cursor_, true);
    return *this;
  }
  constexpr const_iterator operator++(int) noexcept {
    auto old = *this;
    ++*this;
    return old;
  }

  constexpr reference operator*() const noexcept {
    return *cursor_.current->GetKV();
  }
  constexpr pointer operator->() const noexcept {
    return cursor_.current->GetKV();
  }
  constexpr bool operator==(const const_iterator& other) const noexcept {
    return cursor_ == other.cursor_;
  }
  constexpr bool operator!=(const const_iterator& other) const noexcept {
    return !(cursor_ == other.cursor_);
  }

 private:
  friend class HashMap<K, V, H, E>;

  constexpr explicit const_iterator(Cursor cursor) : cursor_(cursor) {}
  constexpr const Cursor& GetCursor() const { return cursor_; }

 private:
  Cursor cursor_;
};

template <class K, class V, class H, class E>
template <class T>
struct HashMap<K, V, H, E>::storage_size {
  inline static constexpr std::size_t kTotalSize = sizeof(std::pair<T, void*>);
  inline static constexpr std::size_t kNearestPowerOfTwo =
      NextPowerOfTwo(kTotalSize);
  // At most 32-bytes being wasted is tolerable.
  inline static constexpr std::size_t kDesiredSize =
      kNearestPowerOfTwo - kTotalSize <= 16 ? kNearestPowerOfTwo : kTotalSize;
  inline static constexpr std::size_t value =
      (kDesiredSize - sizeof(void*)) / alignof(T) * alignof(T);
};

template <class K, class V, class H, class E>
struct HashMap<K, V, H, E>::Node {
  inline static Node* const kEmpty = reinterpret_cast<Node*>(1);

  // Initialized if `next` is NOT `kEmpty`. (Note that if `next` is `nullptr`,
  // this node IS valid.)
  std::aligned_storage_t<storage_size_v<value_type>, alignof(value_type)> kv;
  Node* next = kEmpty;

  ~Node() { FLARE_DCHECK(IsEmpty()); }

  // Initialize fields `key` and `value` with given values. Note that you
  // shouldn't call this method if the node has already been initialized.
  template <class T, class... Args>
  void Accept(T&& k, Args&&... args) {
    FLARE_DCHECK(IsEmpty());
    new (&kv) value_type(std::piecewise_construct,
                         std::forward_as_tuple(std::forward<T>(k)),
                         std::forward_as_tuple(std::forward<Args>(args)...));
    next = nullptr;
  }

  // These accessors are only applicable if the node has been initialized.
  std::pair<const K, V>* GetKV() noexcept {
    FLARE_DCHECK(!IsEmpty());
    return reinterpret_cast<value_type*>(&kv);
  }
  const K& GetKey() noexcept { return GetKV()->first; }
  V* GetValue() noexcept { return &GetKV()->second; }
  bool IsEmpty() const noexcept { return next == kEmpty; }

  // Destroyes what we currently have.
  void Destroy() noexcept {
    FLARE_DCHECK(!IsEmpty());
    GetKV()->~value_type();
    next = kEmpty;  // Likely to be a dead store. TODO(luobogao): Optimize it.
  }
};

template <class K, class V, class H, class E>
inline HashMap<K, V, H, E>::HashMap(std::size_t initial_capacity) {
  // In case size of `Node` can be rounded up to a power of two without wasting
  // more than 32-bytes, we'll do.
  static_assert(NextPowerOfTwo(sizeof(Node)) == sizeof(Node) ||
                NextPowerOfTwo(sizeof(Node)) - sizeof(Node) > 16);
  InitializeFor(NextPowerOfTwo(initial_capacity));
}

template <class K, class V, class H, class E>
inline HashMap<K, V, H, E>::HashMap(HashMap&& other) noexcept : HashMap() {
  swap(other);
}

template <class K, class V, class H, class E>
inline HashMap<K, V, H, E>& HashMap<K, V, H, E>::operator=(
    HashMap&& other) noexcept {
  if (FLARE_UNLIKELY(this == &other)) {
    return *this;
  }
  clear();
  swap(other);
  return *this;
}

template <class K, class V, class H, class E>
inline HashMap<K, V, H, E>::HashMap(const HashMap& other) : HashMap() {
  MergeFrom(other);
}

template <class K, class V, class H, class E>
inline HashMap<K, V, H, E>& HashMap<K, V, H, E>::operator=(
    const HashMap& other) {
  if (FLARE_UNLIKELY(this == &other)) {
    return *this;
  }
  clear();
  MergeFrom(other);
  return *this;
}

template <class K, class V, class H, class E>
inline HashMap<K, V, H, E>::~HashMap() {
  clear();
}

template <class K, class V, class H, class E>
HashMap<K, V, H, E>::HashMap(std::initializer_list<value_type> init)
    : HashMap() {
  for (auto&& [k, v] : init) {
    insert_or_assign(k, v);
  }
}

template <class K, class V, class H, class E>
inline typename HashMap<K, V, H, E>::iterator
HashMap<K, V, H, E>::begin() noexcept {
  return iterator(FirstNonEmptyCursor());
}

template <class K, class V, class H, class E>
inline typename HashMap<K, V, H, E>::const_iterator HashMap<K, V, H, E>::begin()
    const noexcept {
  return const_iterator(FirstNonEmptyCursor());
}

template <class K, class V, class H, class E>
inline typename HashMap<K, V, H, E>::iterator
HashMap<K, V, H, E>::end() noexcept {
  return iterator(SentinelCursor());
}

template <class K, class V, class H, class E>
inline typename HashMap<K, V, H, E>::const_iterator HashMap<K, V, H, E>::end()
    const noexcept {
  return const_iterator(SentinelCursor());
}

template <class K, class V, class H, class E>
void HashMap<K, V, H, E>::clear() noexcept {
  if (!size_) {
    return;
  }
  for (std::size_t index = 0; index != capacity_; ++index) {
    auto current = &nodes_[index];
    if (current->IsEmpty()) {
      continue;  // Try next then.
    }
    auto next = current->next;
    // The first node shouldn't be deleted.
    current->Destroy();
    while ((current = next)) {
      FLARE_CHECK(!current->IsEmpty());
      next = current->next;
      current->Destroy();
      delete current;
    }
  }
  size_ = 0;
}

template <class K, class V, class H, class E>
template <class T, class U>
typename HashMap<K, V, H, E>::iterator HashMap<K, V, H, E>::insert_or_assign(
    T&& key, U&& value) {
  auto&& [idx, ptr] = CreateOrLocateKey(key);
  if (!ptr->IsEmpty()) {
    *ptr->GetValue() = std::forward<U>(value);
  } else {
    ptr->Accept(std::forward<T>(key), std::forward<U>(value));
    FLARE_DCHECK_EQ(GetBucket(ptr->GetKey()), idx);
  }
  FLARE_CHECK(!ptr->IsEmpty());
  return iterator(Cursor{this, idx, ptr});
}

template <class K, class V, class H, class E>
template <class T, class>
inline std::size_t HashMap<K, V, H, E>::erase(const T& key) noexcept {
  if (auto iter = find(key); iter != end()) {
    erase(iter);
    return 1;
  }
  return 0;
}

template <class K, class V, class H, class E>
typename HashMap<K, V, H, E>::iterator HashMap<K, V, H, E>::erase(
    const_iterator pos) noexcept {
  auto cursor = pos.GetCursor();

  // Not that fast to be honest.
  auto current = &nodes_[cursor.bucket_id];
  if (current == cursor.current) {
    auto next = current->next;
    if (!next) {
      ++pos;
    }  // `pos` is NOT updated otherwise, the node itself is updated with the
       // "next" element.
    // The first node in the bucket need to be taken care of specially.
    current->Destroy();
    if (next) {  // There were more than one node in this bucket, since we've
                 // destroyed the first (inlined) one, we have to lift the
                 // second one up.
      // FIXME: It the `const_cast` here U.B.?
      current->Accept(std::move(const_cast<K&>(next->GetKey())),
                      std::move(*next->GetValue()));
      FLARE_DCHECK_EQ(GetBucket(current->GetKey()), cursor.bucket_id);
      current->next = next->next;
      next->Destroy();
      delete next;
    }
  } else {
    ++pos;
    while (current->next != cursor.current) {
      FLARE_CHECK(current->next && !current->next->IsEmpty(),
                  "Invalid iterator?");
      current = current->next;
    }
    current->next = current->next->next;
    cursor.current->Destroy();
    delete cursor.current;
  }

  --size_;  // Someone has gone.
  return iterator(pos.GetCursor());
}

template <class K, class V, class H, class E>
inline void HashMap<K, V, H, E>::swap(HashMap& other) noexcept {
  std::swap(nodes_, other.nodes_);
  std::swap(capacity_, other.capacity_);
  std::swap(rehash_threshold_, other.rehash_threshold_);
  std::swap(mask_, other.mask_);
  std::swap(size_, other.size_);
}

template <class K, class V, class H, class E>
template <class T>
inline V& HashMap<K, V, H, E>::at(const T& key) noexcept {
  auto ptr = TryGet(key);
  FLARE_CHECK(ptr, "Key not found.");
  return *ptr;
}

template <class K, class V, class H, class E>
template <class T>
inline const V& HashMap<K, V, H, E>::at(const T& key) const noexcept {
  auto ptr = TryGet(key);
  FLARE_CHECK(ptr, "Key not found.");
  return *ptr;
}

template <class K, class V, class H, class E>
template <class T>
inline V& HashMap<K, V, H, E>::operator[](T&& key) {
  auto&& [idx, ptr] = CreateOrLocateKey(key);
  if (ptr->IsEmpty()) {
    ptr->Accept(std::forward<T>(key));
    FLARE_DCHECK_EQ(GetBucket(ptr->GetKey()), idx);
  }
  FLARE_DCHECK(!ptr->IsEmpty());
  return *ptr->GetValue();
}

template <class K, class V, class H, class E>
template <class T>
inline typename HashMap<K, V, H, E>::iterator HashMap<K, V, H, E>::find(
    const T& key) noexcept {
  auto&& [idx, ptr] = LocateKey(key);
  return iterator(Cursor{this, idx, ptr});
}

template <class K, class V, class H, class E>
template <class T>
inline typename HashMap<K, V, H, E>::const_iterator HashMap<K, V, H, E>::find(
    const T& key) const noexcept {
  auto&& [idx, ptr] = LocateKey(key);
  return iterator(Cursor{this, idx, ptr});
}

template <class K, class V, class H, class E>
template <class T>
inline bool HashMap<K, V, H, E>::contains(const T& key) const noexcept {
  return find(key) != end();
}

template <class K, class V, class H, class E>
template <class T>
inline V* HashMap<K, V, H, E>::TryGet(const T& key) noexcept {
  auto bucket = &nodes_[GetBucket(key)];
  if (bucket->IsEmpty()) {
    return nullptr;
  }
  if (E()(bucket->GetKey(), key)) {
    return bucket->GetValue();
  }
  return TryGetSlow(bucket->next, key);
}

template <class K, class V, class H, class E>
template <class T>
inline const V* HashMap<K, V, H, E>::TryGet(const T& key) const noexcept {
  return const_cast<HashMap*>(this)->TryGet(key);
}

template <class K, class V, class H, class E>
void HashMap<K, V, H, E>::InitializeFor(std::size_t capacity) {
  FLARE_CHECK_GE(capacity, 4, "The minimum `capacity` is 4.");
  FLARE_CHECK_EQ(capacity & (capacity - 1), 0,
                 "`capacity` must be a power of 2.");
  capacity_ = capacity;
  mask_ = capacity_ - 1;
  rehash_threshold_ = capacity_ * 3 / 4;
  size_ = 0;
  nodes_ = std::make_unique<Node[]>(capacity_);
}

template <class K, class V, class H, class E>
template <class T>
inline std::size_t HashMap<K, V, H, E>::GetBucket(const T& key) const {
  auto hash = H()(key);
  return hash & mask_;
}

template <class K, class V, class H, class E>
void HashMap<K, V, H, E>::Rehash() {
  HashMap doubled(capacity_ * 2);
  for (auto&& [k, v] : *this) {
    // FIXME: It the `const_cast` here U.B.?
    doubled.insert_or_assign(std::move(const_cast<K&>(k)), std::move(v));
  }
  FLARE_CHECK_EQ(doubled.size(), size());
  swap(doubled);
}

template <class K, class V, class H, class E>
inline void HashMap<K, V, H, E>::RehashIfNecessary() {
  if (FLARE_UNLIKELY(size_ >= rehash_threshold_)) {
    Rehash();
  }
}

template <class K, class V, class H, class E>
void HashMap<K, V, H, E>::MergeFrom(const HashMap& other) {
  for (auto&& [k, v] : other) {
    insert_or_assign(k, v);
  }
}

template <class K, class V, class H, class E>
typename HashMap<K, V, H, E>::Cursor HashMap<K, V, H, E>::FirstNonEmptyCursor()
    const noexcept {
  if (empty()) {
    return SentinelCursor();
  }
  auto cursor = Cursor{this, 0, &nodes_[0]};
  if (nodes_[0].IsEmpty()) {
    NextCursor(&cursor, false);
  }
  return cursor;
}

template <class K, class V, class H, class E>
void HashMap<K, V, H, E>::NextCursor(Cursor* cursor,
                                     bool initialized) const noexcept {
  if (initialized && cursor->current->next) {
    FLARE_CHECK(!cursor->current->next->IsEmpty());
    cursor->current = cursor->current->next;
    return;
  }
  for (std::size_t index = cursor->bucket_id + 1; index != capacity_; ++index) {
    auto&& e = nodes_[index];
    if (!e.IsEmpty()) {
      cursor->bucket_id = index;
      cursor->current = &e;
      return;
    }
  }
  *cursor = SentinelCursor();
}

template <class K, class V, class H, class E>
inline typename HashMap<K, V, H, E>::Cursor
HashMap<K, V, H, E>::SentinelCursor() const noexcept {
  return Cursor{this, capacity_, nullptr};
}

template <class K, class V, class H, class E>
template <class T>
inline typename HashMap<K, V, H, E>::Location HashMap<K, V, H, E>::LocateKey(
    const T& key) const noexcept {
  auto idx = GetBucket(key);
  auto current = &nodes_[idx];
  if (current->IsEmpty()) {
    return Location{capacity_, nullptr};  // @sa: SentinelCursor().
  }
  if (E()(current->GetKey(), key)) {
    return Location{idx, current};
  }
  if (current->next == nullptr) {  // `current` CHANGED.
    return Location{capacity_, nullptr};
  }
  return LocateKeySlow(key, idx, current->next);
}

template <class K, class V, class H, class E>
template <class T>
[[gnu::noinline]] typename HashMap<K, V, H, E>::Location
HashMap<K, V, H, E>::LocateKeySlow(const T& key, std::size_t idx,
                                   Node* current) const noexcept {
  do {
    FLARE_CHECK(!current->IsEmpty());
    if (E()(current->GetKey(), key)) {
      return Location{idx, current};
    }
    current = current->next;
  } while (current);
  return Location{capacity_, nullptr};  // @sa: SentinelCursor().
}

template <class K, class V, class H, class E>
template <class T>
inline typename HashMap<K, V, H, E>::Location
HashMap<K, V, H, E>::CreateOrLocateKey(const T& key) {
  // Rehash is done beforehand so that we don't have to take it into
  // consideration when constructing iterators.
  RehashIfNecessary();

  auto idx = GetBucket(key);
  auto current = &nodes_[idx];
  // We don't expect to see too many collisions, so either the inline node is
  // empty, or the key should match.
  if (current->IsEmpty()) {
    ++size_;
    return Location{idx, current};
  }
  if (E()(current->GetKey(), key)) {
    return Location{idx, current};
  } else if (!current->next) {  // Not very likely but collision does occur.
    current = current->next = new Node();
    ++size_;
    return Location{idx, current};
  }
  return CreateOrLocateKeySlow(key, idx, current);  // Really bad day.
}

template <class K, class V, class H, class E>
template <class T>
typename HashMap<K, V, H, E>::Location
HashMap<K, V, H, E>::CreateOrLocateKeySlow(const T& key, std::size_t idx,
                                           Node* current) {
  // Rehash is checked by `CreateOrLocateKey`. Don't bother doing that here.
  while (true) {
    // For the first call this should hold as well, otherwise
    // `CreateOrLocateKey` shouldn't have called us.
    FLARE_CHECK(!current->IsEmpty());
    if (E()(current->GetKey(), key)) {
      return Location{idx, current};
    }
    if (current->next) {
      current = current->next;
    } else {
      // Reached the end of the list. Create a new node and bail out then.
      current = current->next = new Node();
      ++size_;
      return Location{idx, current};
    }
  }
}

template <class K, class V, class H, class E>
template <class T>
[[gnu::noinline]] V* HashMap<K, V, H, E>::TryGetSlow(Node* bucket,
                                                     const T& key) noexcept {
  while (bucket) {
    FLARE_CHECK(!bucket->IsEmpty());
    if (E()(bucket->GetKey(), key)) {
      return bucket->GetValue();
    }
    bucket = bucket->next;
  }
  return nullptr;
}

template <class K, class V, class H, class E>
constexpr std::size_t HashMap<K, V, H, E>::NextPowerOfTwo(std::size_t v) {
  // @sa: https://stackoverflow.com/a/466242
  --v;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v |= v >> 32;
  ++v;
  return v;
}

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_HASH_MAP_H_
