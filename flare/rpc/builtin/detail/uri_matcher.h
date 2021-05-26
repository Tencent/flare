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

#ifndef FLARE_RPC_BUILTIN_DETAIL_URI_MATCHER_H_
#define FLARE_RPC_BUILTIN_DETAIL_URI_MATCHER_H_

#include <regex>
#include <string>
#include <string_view>

#include "flare/base/function.h"

namespace flare::detail {

// This class matches URI using one of the following methods:
//
// - If the class was instantiated with a `std::string`, it matches URI by
//   prefix.
//
// - If the class was instantiated with a `std::regex`, that regular expression
//   is used.
//
// - If a user callback was provided, the callback is used.
//
// - Otherwise (a default-constructed one) all URIs are matched.
class UriMatcher {
 public:
  // Default-constructed matcher matches all URIs.
  UriMatcher();

  // Matches URIs with the given prefix.
  /* implicit */ UriMatcher(std::string prefix);

  // We need these two overloads, otherwise using us as constructor argument is
  // a pain (`const char*` can't be implicitly converted to `UriMatcher` in that
  // case, as two implicit conversions are required.).
  /* implicit */ UriMatcher(const char* prefix);
  /* implicit */ UriMatcher(const std::string_view& prefix);

  // Matches URIs using the regular expression given.
  /* implicit */ UriMatcher(std::regex uri_matcher);

  // Matches URIs with user provided callback.
  /* implicit */ UriMatcher(
      Function<bool(const std::string_view&)> uri_matcher);

  bool operator()(const std::string_view& uri) const { return matcher_(uri); }

 private:
  Function<bool(const std::string_view&)> matcher_;
};

}  // namespace flare::detail

#endif  // FLARE_RPC_BUILTIN_DETAIL_URI_MATCHER_H_
