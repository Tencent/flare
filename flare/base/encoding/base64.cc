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

#include "flare/base/encoding/base64.h"

#include <limits>
#include <string>
#include <string_view>

#include "openssl/evp.h"

#include "flare/base/logging.h"

namespace flare {

std::string EncodeBase64(std::string_view from) {
  std::string result;
  EncodeBase64(from, &result);
  return result;
}

std::optional<std::string> DecodeBase64(std::string_view from) {
  std::string result;
  if (!DecodeBase64(from, &result)) {
    return std::nullopt;
  }
  return result;
}

void EncodeBase64(std::string_view from, std::string* to) {
  FLARE_CHECK(from.size() < std::numeric_limits<int>::max(),
              "Not implemented: Source bytes too long.");

  auto size = 4 * ((from.size() + 2) / 3);
  int rc;
  to->resize(size + 1 /* Terminating null added by EVP_EncodeBlock */);
  rc = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(to->data()),
                       reinterpret_cast<const unsigned char*>(from.data()),
                       from.size());
  // Why should it even fail?
  FLARE_CHECK_EQ(rc, size, "Unexpected: Failed to do base64-encode.");
  to->pop_back();  // Terminating null.
}

bool DecodeBase64(std::string_view from, std::string* to) {
  if (from.size() < 3) {
    if (from.empty()) {
      to->clear();
      return true;
    }
    // Invalid text otherwise.
    return false;
  }
  auto size = 3 * from.size() / 4;
  int rc;
  to->resize(size + 1);
  rc = EVP_DecodeBlock(reinterpret_cast<unsigned char*>(to->data()),
                       reinterpret_cast<const unsigned char*>(from.data()),
                       from.size());
  if (rc != size) {
    return false;
  }

  // @sa: https://tools.ietf.org/html/rfc4648#section-4
  //
  // (1) The final quantum of encoding input is an integral multiple of 24
  //     bits; here, the final unit of encoded output will be an integral
  //     multiple of 4 characters with no "=" padding.
  //
  // (2) The final quantum of encoding input is exactly 8 bits; here, the
  //     final unit of encoded output will be two characters followed by
  //     two "=" padding characters.
  //
  // (3) The final quantum of encoding input is exactly 16 bits; here, the
  //     final unit of encoded output will be three characters followed by
  //     one "=" padding character.
  FLARE_CHECK(from.size() >= 2 && to->size() >= 2);
  if (from[from.size() - 1] == '=') {
    to->pop_back();
  }
  if (from[from.size() - 2] == '=') {
    to->pop_back();
  }
  to->pop_back();
  return true;
}

}  // namespace flare
