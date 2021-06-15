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

#ifndef FLARE_BASE_CRYPTO_BLAKE3_H_
#define FLARE_BASE_CRYPTO_BLAKE3_H_

#include <initializer_list>
#include <string>
#include <string_view>

#include "blake3/blake3.h"

#include "flare/base/buffer.h"

namespace flare {

// Hash `data` using BLAKE3 algorithm.
//
// The resulting value is NOT hex-encoded.
std::string Blake3(const std::string_view& data);
std::string Blake3(std::initializer_list<std::string_view> data);
std::string Blake3(const NoncontiguousBuffer& data);

// This class allows you to generated BLAKE3 hash of a stream of data.
class Blake3Digest {
 public:
  Blake3Digest();

  void Append(const std::string_view& data) noexcept;
  void Append(const void* data, std::size_t length) noexcept;
  void Append(std::initializer_list<std::string_view> data) noexcept;

  template <
      template <class> class C, class T,
      std::enable_if_t<std::is_convertible_v<T, std::string_view>>* = nullptr>
  void Append(const C<T>& data) noexcept {
    for (auto&& e : data) {
      Append(e);
    }
  }

  std::string DestructiveGet() noexcept;

 private:
  blake3_hasher state_;
};

}  // namespace flare

#endif  // FLARE_BASE_CRYPTO_BLAKE3_H_
