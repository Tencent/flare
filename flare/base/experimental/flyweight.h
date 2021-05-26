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

#ifndef FLARE_BASE_EXPERIMENTAL_FLYWEIGHT_H_
#define FLARE_BASE_EXPERIMENTAL_FLYWEIGHT_H_

#include <functional>
#include <initializer_list>
#include <mutex>
#include <unordered_set>

#include "flare/base/never_destroyed.h"

namespace flare::experimental {

template <class T>
class Flyweight;

namespace flyweight {

// Repository is responsible for instantiating objects and keeping them alive.
//
// Threas-safety is taken care of by `Flyweight`, this class by itself is not
// thread-safe.
template <class T, class... ExtraSetArgs>
class DefaultRepository {
 public:
  template <class... Args>
  const T* GetExistingOrNew(Args&&... args);

  // TODO(luobogao): Erase?
 private:
  std::unordered_set<T, ExtraSetArgs...> objects_;
};

namespace detail {

template <class T, class... Args>
Flyweight<T> MakeFlyweightWith(Args&&... args);

}

}  // namespace flyweight

template <class T, class = void>
struct FlyweightTraits {
  using Repository = flyweight::DefaultRepository<T>;
};

// Flyweight permits lightweight sharing objects that are costly to copy.
//
// Copying / destroying flyweights is cheap.
template <class T>
class Flyweight {
 public:
  Flyweight() = default;

  // Accessor.
  //
  // Here we provide a pointer-like interface. This is different from what
  // boost.flyweight does. Boost.flyweight provides `const T& Get()` and
  // conversion operator. I'm not sure which design is superior.
  const T* Get() const noexcept { return ptr_; }
  const T& operator*() const noexcept { return *Get(); }
  const T* operator->() const noexcept { return Get(); }

  explicit operator bool() const noexcept { return ptr_; }

 private:
  template <class U, class... Args>
  friend Flyweight<U> flyweight::detail::MakeFlyweightWith(Args&&... args);

  explicit Flyweight(const T* existing) : ptr_(existing) {}

 private:
  const T* ptr_ = nullptr;
};

// TODO(luobogao): Implement three-way comparison instead.

// All comparision operatrors work on internal pointer, not the object itself.
//
// This should "just work" for (in)equality comparison, but not ordering
// comparisons.
//
// The reason why it would work for equality comparision is that if `left` and
// `right` are equivalent, unless `Repository` itself is buggy at
// de-duplication, their pointers must match.
template <class T>
bool operator==(Flyweight<T> left, Flyweight<T> right);
template <class T>
bool operator!=(Flyweight<T> left, Flyweight<T> right);
template <class T>
bool operator<(Flyweight<T> left, Flyweight<T> right);

// Make a flyweight with the given arguments. This method can be costly as it
// always instantiates an instance of `T`, even when not required (in this case
// the just-instantiated instance is dropped).
template <class T, class... Args>
Flyweight<T> MakeFlyweight(Args&&... args);

}  // namespace flare::experimental

namespace std {

template <class T>
struct hash<flare::experimental::Flyweight<T>> {
  std::size_t operator()(
      const flare::experimental::Flyweight<T>& object) const noexcept {
    return reinterpret_cast<std::uintptr_t>(object.Get());
  }
};

}  // namespace std

//////////////////////////////////
// Implementation goes below.   //
//////////////////////////////////

namespace flare::experimental {

namespace flyweight {

template <class T, class... ExtraSetArgs>
template <class... Args>
const T* DefaultRepository<T, ExtraSetArgs...>::GetExistingOrNew(
    Args&&... args) {
  return &*objects_.emplace(std::forward<Args>(args)...).first;
}

namespace detail {

template <class T>
struct InterlockedRepository {
  std::mutex lock;
  typename FlyweightTraits<T>::Repository repository;
};

template <class T>
InterlockedRepository<T>* GetRepositoryFor() {
  static NeverDestroyed<InterlockedRepository<T>> repository;
  return repository.Get();
}

template <class T, class... Args>
Flyweight<T> MakeFlyweightWith(Args&&... args) {
  auto repository = GetRepositoryFor<T>();
  std::scoped_lock _(repository->lock);
  return Flyweight<T>(
      repository->repository.GetExistingOrNew(std::forward<Args>(args)...));
}

}  // namespace detail

}  // namespace flyweight

template <class T, class... Args>
Flyweight<T> MakeFlyweight(Args&&... args) {
  return flyweight::detail::MakeFlyweightWith<T>(std::forward<Args>(args)...);
}

template <class T, class V>
Flyweight<T> MakeFlyweight(std::initializer_list<V> list) {
  return flyweight::detail::MakeFlyweightWith<T>(list);
}

template <class T>
bool operator==(Flyweight<T> left, Flyweight<T> right) {
  return left.Get() == right.Get();
}

template <class T>
bool operator!=(Flyweight<T> left, Flyweight<T> right) {
  return left.Get() != right.Get();
}

template <class T>
bool operator<(Flyweight<T> left, Flyweight<T> right) {
  return left.Get() < right.Get();
}

}  // namespace flare::experimental

#endif  // FLARE_BASE_EXPERIMENTAL_FLYWEIGHT_H_
