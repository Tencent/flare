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

#ifndef FLARE_BASE_HANDLE_H_
#define FLARE_BASE_HANDLE_H_

#include <unistd.h>

#include "flare/base/logging.h"

namespace flare {

namespace detail {

struct HandleDeleter {
  void operator()(int fd) const noexcept {
    if (fd == 0 || fd == -1) {
      return;
    }
    FLARE_PCHECK(::close(fd) == 0);
  }
};

// Using inheritance rather than member method for EBO.
//
// @sa: https://en.cppreference.com/w/cpp/language/attributes/no_unique_address
//
// Not sure if we should move this class out for public use.
template <class T, class Deleter, T... kInvalidHandles>
class GenericHandle : private Deleter {
  // Anyone will do.
  static constexpr T kDefaultInvalid = (kInvalidHandles, ...);

 public:
  GenericHandle() = default;

  constexpr explicit GenericHandle(T handle) noexcept : handle_(handle) {}
  ~GenericHandle() { Reset(); }

  // Movable.
  constexpr GenericHandle(GenericHandle&& h) noexcept {
    handle_ = h.handle_;
    h.handle_ = kDefaultInvalid;
  }
  constexpr GenericHandle& operator=(GenericHandle&& h) noexcept {
    handle_ = h.handle_;
    h.handle_ = kDefaultInvalid;
    return *this;
  }

  // Non-copyable.
  GenericHandle(const GenericHandle&) = delete;
  GenericHandle& operator=(const GenericHandle&) = delete;

  // Useful if handle is returned by argument:
  //
  // GetHandle(..., h.Retrieve());
  T* Retrieve() noexcept { return &handle_; }

  // Return handle's value.
  constexpr T Get() const noexcept { return handle_; }

  // Return if we're holding a valid handle value.
  constexpr explicit operator bool() const noexcept {
    // The "extra" parentheses is grammatically required.
    return ((handle_ != kInvalidHandles) && ...);
  }

  // Return handle's value, and give up ownership.
  constexpr T Leak() noexcept {
    int rc = handle_;
    handle_ = kDefaultInvalid;
    return rc;
  }

  void Reset(T new_value = kDefaultInvalid) noexcept {
    if (operator bool()) {
      Deleter::operator()(handle_);
    }
    handle_ = new_value;
  }

 private:
  T handle_ = kDefaultInvalid;
};

}  // namespace detail

using Handle = detail::GenericHandle<int, detail::HandleDeleter, -1, 0>;

}  // namespace flare

#endif  // FLARE_BASE_HANDLE_H_
