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

#ifndef FLARE_BASE_TYPE_INDEX_H_
#define FLARE_BASE_TYPE_INDEX_H_

#include <functional>
#include <typeindex>

namespace flare {

namespace detail {

// For each type, there is only one instance of `TypeIndexEntity`. `TypeIndex`
// keeps a reference to the entity, which is used for comparison and other
// stuff.
struct TypeIndexEntity {
  std::type_index runtime_type_index;
};

template <class T>
inline const TypeIndexEntity kTypeIndexEntity{std::type_index(typeid(T))};

}  // namespace detail

// Due to QoI issue in libstdc++, which uses `strcmp` for comparing
// `std::type_index`, we roll our own.
//
// Note our own does NOT support runtime type, only compile time type is
// recognized.
class TypeIndex {
 public:
  constexpr TypeIndex() noexcept : entity_(nullptr) {}

  // In case you need `std::type_index` of the corresponding type, this method
  // is provided for you convenience.
  //
  // Keep in mind, though, that this method can be slow. In most cases you
  // should only use it for logging purpose.
  std::type_index GetRuntimeTypeIndex() const {
    return entity_->runtime_type_index;
  }

  constexpr bool operator==(TypeIndex t) const noexcept {
    return entity_ == t.entity_;
  }
  constexpr bool operator!=(TypeIndex t) const noexcept {
    return entity_ != t.entity_;
  }
  constexpr bool operator<(TypeIndex t) const noexcept {
    return entity_ < t.entity_;
  }

  // If C++20 is usable, you get all other comparison operators (!=, <=, >, ...)
  // automatically. I'm not gonna implement them as we don't use them for the
  // time being.

 private:
  template <class T>
  friend constexpr TypeIndex GetTypeIndex();
  friend struct std::hash<TypeIndex>;

  constexpr explicit TypeIndex(const detail::TypeIndexEntity* entity) noexcept
      : entity_(entity) {}

 private:
  const detail::TypeIndexEntity* entity_;
};

template <class T>
constexpr TypeIndex GetTypeIndex() {
  return TypeIndex(&detail::kTypeIndexEntity<T>);
}

}  // namespace flare

namespace std {

template <>
struct hash<flare::TypeIndex> {
  size_t operator()(const flare::TypeIndex& type) const noexcept {
    return std::hash<const void*>{}(type.entity_);
  }
};

}  // namespace std

#endif  // FLARE_BASE_TYPE_INDEX_H_
