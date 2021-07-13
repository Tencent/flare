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

#include "flare/rpc/builtin/detail/uri_matcher.h"

#include <regex>
#include <string>
#include <string_view>
#include <utility>

#include "flare/base/string.h"

namespace flare::detail {

UriMatcher::UriMatcher() : matcher_([](auto&&) { return true; }) {}

UriMatcher::UriMatcher(const char* prefix) : UriMatcher(std::string(prefix)) {}

UriMatcher::UriMatcher(std::string_view prefix)
    : UriMatcher(std::string(prefix)) {}

UriMatcher::UriMatcher(std::string prefix)
    : matcher_([prefix = std::move(prefix)](auto&& s) {
        return StartsWith(s, prefix);
      }) {}

UriMatcher::UriMatcher(std::regex uri_matcher)
    : matcher_([uri_matcher = std::move(uri_matcher)](auto&& s) {
        return std::regex_match(s.begin(), s.end(), uri_matcher);
      }) {}

UriMatcher::UriMatcher(Function<bool(std::string_view)> uri_matcher)
    : matcher_(std::move(uri_matcher)) {}

}  // namespace flare::detail
