// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_BASE_CRYPTO_MD5_H_
#define FLARE_BASE_CRYPTO_MD5_H_

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

#include "flare/base/buffer.h"

namespace flare {

// Hash `data` using MD5.
//
// The resulting value is NOT hex-encoded.
std::string Md5(std::string_view data);
std::string Md5(std::initializer_list<std::string_view> data);
std::string Md5(const NoncontiguousBuffer& data);

// HMAC-MD5
std::string HmacMd5(std::string_view key, std::string_view data);
std::string HmacMd5(std::string_view key,
                    std::initializer_list<std::string_view> data);
std::string HmacMd5(std::string_view key, const NoncontiguousBuffer& data);

}  // namespace flare

#endif  // FLARE_BASE_CRYPTO_MD5_H_
