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

#ifndef FLARE_BASE_ENCODING_HEX_H_
#define FLARE_BASE_ENCODING_HEX_H_

#include <optional>
#include <string>
#include <string_view>

namespace flare {

std::string EncodeHex(const std::string_view& from, bool uppercase = false);
std::optional<std::string> DecodeHex(const std::string_view& from);

void EncodeHex(const std::string_view& from, std::string* to,
               bool uppercase = false);
bool DecodeHex(const std::string_view& from, std::string* to);

}  // namespace flare

#endif  // FLARE_BASE_ENCODING_HEX_H_
