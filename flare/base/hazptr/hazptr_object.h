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

#ifndef FLARE_BASE_HAZPTR_HAZPTR_OBJECT_H_
#define FLARE_BASE_HAZPTR_HAZPTR_OBJECT_H_

#include <memory>
#include <utility>

#include "flare/base/hazptr/hazptr_domain.h"
#include "flare/base/internal/logging.h"

namespace flare {

template <class T, class D = std::default_delete<T>>
class HazptrObject;
class HazptrDomain;

namespace hazptr {

// "Real" base class for `Hazptr`-enabled classes.
//
// FOR INTERNAL USE. USE `HazptrObject<T, D>` INSTEAD.
class Object {
 private:
  template <class T, class D>
  friend class flare::HazptrObject;
  friend class flare::HazptrDomain;

  constexpr Object() = default;
  virtual ~Object() = default;  // It making dtor `virtual` acceptable?

  // Nothing is copied / moved.
  constexpr Object(const Object&) {}
  constexpr Object(Object&&) noexcept {}
  constexpr Object& operator=(const Object&) { return *this; }
  constexpr Object& operator=(Object&&) noexcept { return *this; }

  // Free resources allocated for this object.
  virtual void DestroySelf() noexcept = 0;

  // Check for double free. For debugging purpose only.
  void PreRetireCheck() { FLARE_CHECK_EQ(next_, this); }
  void PushRetired(HazptrDomain* domain);

 private:
  Object* next_ = {this};
};

}  // namespace hazptr

// For your objects to be hazard-pointer-enabled, you need to inherit from this
// base class.
template <class T, class D>
class HazptrObject : public hazptr::Object {
 public:  // protected?
  // Retire the object. The object will be destroyed by `deleter` (after an
  // nondeterministic time period.).
  //
  // Note that before calling this method, you must make sure that no MORE
  // reference to it can be made. (Normally you store the pointer in an
  // `std::atomic<...>` variable, and calls `Retire` after change the atomic
  // variable points to another object.)
  void Retire(D deleter = {}, HazptrDomain* domain = GetDefaultHazptrDomain()) {
    PreRetireCheck();
    deleter_ = std::move(deleter);
    PushRetired(domain);
  }

 private:
  void DestroySelf() noexcept override { deleter_(static_cast<T*>(this)); }

 private:
  /* [[no_unique_address]] */ D deleter_;
};

}  // namespace flare

#endif  // FLARE_BASE_HAZPTR_HAZPTR_OBJECT_H_
