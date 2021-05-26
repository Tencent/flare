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

#ifndef FLARE_BASE_CALLBACK_H_
#define FLARE_BASE_CALLBACK_H_

#include <utility>

#include "thirdparty/protobuf/service.h"  // ...

// You shouldn't be using `google::protobuf::Closure`..
//
// @sa: https://github.com/protocolbuffers/protobuf/issues/1505

namespace flare {

namespace detail {

template <class Impl, bool kSelfDestroying>
class Callback : public google::protobuf::Closure {
 public:
  template <class T>
  explicit Callback(T&& impl) : impl_(std::forward<T>(impl)) {}

  void Run() override {
    impl_();
    if (kSelfDestroying) {
      delete this;
    }
  }

 private:
  Impl impl_;
};

}  // namespace detail

namespace internal {

template <class T>
class LocalCallback : public google::protobuf::Closure {
 public:
  explicit LocalCallback(T&& impl) : impl_(std::forward<T>(impl)) {}

  void Run() override { impl_(); }

 private:
  T impl_;
};

}  // namespace internal

template <class F>
google::protobuf::Closure* NewCallback(F&& f) {
  return new detail::Callback<std::remove_reference_t<F>, true>(
      std::forward<F>(f));
}

template <class F>
google::protobuf::Closure* NewPermanentCallback(F&& f) {
  return new detail::Callback<std::remove_reference_t<F>, false>(
      std::forward<F>(f));
}

}  // namespace flare

#endif  // FLARE_BASE_CALLBACK_H_
