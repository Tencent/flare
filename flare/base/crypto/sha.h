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

#ifndef FLARE_BASE_CRYPTO_SHA_H_
#define FLARE_BASE_CRYPTO_SHA_H_

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

#include "flare/base/buffer.h"

namespace flare {

// Hash `data` using Secure Hash Algorithms.
//
// The resulting value is NOT hex-encoded.
std::string Sha1(const std::string_view& data);
std::string Sha1(std::initializer_list<std::string_view> data);
std::string Sha1(const NoncontiguousBuffer& data);
std::string Sha224(const std::string_view& data);
std::string Sha224(std::initializer_list<std::string_view> data);
std::string Sha224(const NoncontiguousBuffer& data);
std::string Sha256(const std::string_view& data);
std::string Sha256(std::initializer_list<std::string_view> data);
std::string Sha256(const NoncontiguousBuffer& data);
std::string Sha384(const std::string_view& data);
std::string Sha384(std::initializer_list<std::string_view> data);
std::string Sha384(const NoncontiguousBuffer& data);
std::string Sha512(const std::string_view& data);
std::string Sha512(std::initializer_list<std::string_view> data);
std::string Sha512(const NoncontiguousBuffer& data);

// HMAC-SHA
std::string HmacSha1(const std::string_view& key, const std::string_view& data);
std::string HmacSha1(const std::string_view& key,
                     std::initializer_list<std::string_view> data);
std::string HmacSha1(const std::string_view& key,
                     const NoncontiguousBuffer& data);
std::string HmacSha224(const std::string_view& key,
                       const std::string_view& data);
std::string HmacSha224(const std::string_view& key,
                       std::initializer_list<std::string_view> data);
std::string HmacSha224(const std::string_view& key,
                       const NoncontiguousBuffer& data);
std::string HmacSha256(const std::string_view& key,
                       const std::string_view& data);
std::string HmacSha256(const std::string_view& key,
                       std::initializer_list<std::string_view> data);
std::string HmacSha256(const std::string_view& key,
                       const NoncontiguousBuffer& data);
std::string HmacSha384(const std::string_view& key,
                       const std::string_view& data);
std::string HmacSha384(const std::string_view& key,
                       std::initializer_list<std::string_view> data);
std::string HmacSha384(const std::string_view& key,
                       const NoncontiguousBuffer& data);
std::string HmacSha512(const std::string_view& key,
                       const std::string_view& data);
std::string HmacSha512(const std::string_view& key,
                       std::initializer_list<std::string_view> data);
std::string HmacSha512(const std::string_view& key,
                       const NoncontiguousBuffer& data);

}  // namespace flare

#endif  // FLARE_BASE_CRYPTO_SHA_H_
