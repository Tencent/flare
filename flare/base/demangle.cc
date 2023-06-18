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

#include "flare/base/demangle.h"

#ifdef __GNUC__
#include <cxxabi.h>
#endif

#include <string>

#include "flare/base/deferred.h"

namespace flare {

std::string Demangle(const char* s) {
#ifdef _MSC_VER
  return s;
#elif defined(__GNUC__)
  [[maybe_unused]] int status;
  auto demangled = abi::__cxa_demangle(s, nullptr, nullptr, &status);
  ScopedDeferred _([&] { free(demangled); });
  if (!demangled) {
    return s;
  }
  return demangled;
#else
#error "Demangle not supported on current compiler."
#endif
}

}  // namespace flare
