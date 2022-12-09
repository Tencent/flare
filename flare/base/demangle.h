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

#ifndef FLARE_BASE_DEMANGLE_H_
#define FLARE_BASE_DEMANGLE_H_

#include <string>
#include <typeinfo>
#include <utility>

namespace flare {

std::string Demangle(const char* s);

template <class T>
std::string GetTypeName() {
  return Demangle(typeid(T).name());
}

#if __GNUC__ == 12
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#endif

template <class T>
std::string GetTypeName(T&& o) {
  return Demangle(typeid(std::forward<T>(o)).name());
}

#if __GNUC__ == 12
#pragma GCC diagnostic pop
#endif

}  // namespace flare

#endif  // FLARE_BASE_DEMANGLE_H_
