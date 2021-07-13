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

#ifndef FLARE_BASE_CRYPTO_DETAIL_OPENSSL_IMPL_H_
#define FLARE_BASE_CRYPTO_DETAIL_OPENSSL_IMPL_H_

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

#include "openssl/hmac.h"

#include "flare/base/buffer.h"
#include "flare/base/logging.h"

// This file implements Secure Hash Algorithms using OpenSSL.

namespace flare::detail {

template <auto Init, auto Update, auto Fini, class Context,
          std::size_t kBufferSize, class F>
inline std::string MessageDigestWithCallbackImpl(F&& cb) {
  Context ctx;
  FLARE_CHECK_EQ(Init(&ctx), 1);
  cb([&](std::string_view buffer) {
    FLARE_CHECK_EQ(Update(&ctx, buffer.data(), buffer.size()), 1);
  });

  unsigned char buffer[kBufferSize];  // NOLINT.
  FLARE_CHECK_EQ(Fini(buffer, &ctx), 1);
  return std::string(reinterpret_cast<char*>(buffer), kBufferSize);
}

template <std::size_t kBufferSize, class F>
inline std::string HmacWithCallbackImpl(const EVP_MD* evp_md,
                                        std::string_view key, F&& cb) {
  HMAC_CTX ctx;
  HMAC_CTX_init(&ctx);
  FLARE_CHECK_EQ(HMAC_Init_ex(&ctx, key.data(), key.size(), evp_md, nullptr),
                 1);
  cb([&](std::string_view buffer) {
    auto ptr =
        reinterpret_cast<const unsigned char*>(buffer.data());  // NOLINT.
    FLARE_CHECK_EQ(HMAC_Update(&ctx, ptr, buffer.size()), 1);
  });

  unsigned char buffer[kBufferSize];  // NOLINT.
  FLARE_CHECK_EQ(HMAC_Final(&ctx, buffer, nullptr), 1);
  HMAC_CTX_cleanup(&ctx);
  return std::string(reinterpret_cast<char*>(buffer), kBufferSize);
}

template <auto Init, auto Update, auto Fini, class Context,
          std::size_t kBufferSize>
std::string MessageDigestImpl(std::string_view data) {
  return MessageDigestWithCallbackImpl<Init, Update, Fini, Context,
                                       kBufferSize>(
      [&](auto&& cb) { cb(data); });
}

template <auto Init, auto Update, auto Fini, class Context,
          std::size_t kBufferSize, class Buffers>
std::string MessageDigestImpl(const Buffers& data) {
  return MessageDigestWithCallbackImpl<Init, Update, Fini, Context,
                                       kBufferSize>([&](auto&& cb) {
    for (auto&& e : data) {
      cb({e.data(), e.size()});
    }
  });
}

template <std::size_t kBufferSize>
std::string HmacImpl(const EVP_MD* evp_md, std::string_view key,
                     std::string_view data) {
  return HmacWithCallbackImpl<kBufferSize>(evp_md, key,
                                           [&](auto&& cb) { cb(data); });
}

template <std::size_t kBufferSize, class Buffers>
std::string HmacImpl(const EVP_MD* evp_md, std::string_view key,
                     const Buffers& data) {
  return HmacWithCallbackImpl<kBufferSize>(evp_md, key, [&](auto&& cb) {
    for (auto&& e : data) {
      cb({e.data(), e.size()});
    }
  });
}

#define FLARE_DETAIL_CRYPTO_DEFINE_HASH_AND_HMAC_IMPL_FOR(                     \
    MethodName, OpenSSLMethodPrefix, OpenSSLContextPrefix,                     \
    OpenSSLOutputSizePrefix, EVPSuffix)                                        \
  std::string MethodName(std::string_view data) {                              \
    return detail::MessageDigestImpl<                                          \
        OpenSSLMethodPrefix##_Init, OpenSSLMethodPrefix##_Update,              \
        OpenSSLMethodPrefix##_Final, OpenSSLContextPrefix##_CTX,               \
        OpenSSLOutputSizePrefix##_DIGEST_LENGTH>(data);                        \
  }                                                                            \
  std::string MethodName(std::initializer_list<std::string_view> data) {       \
    return detail::MessageDigestImpl<                                          \
        OpenSSLMethodPrefix##_Init, OpenSSLMethodPrefix##_Update,              \
        OpenSSLMethodPrefix##_Final, OpenSSLContextPrefix##_CTX,               \
        OpenSSLOutputSizePrefix##_DIGEST_LENGTH>(data);                        \
  }                                                                            \
  std::string MethodName(const NoncontiguousBuffer& data) {                    \
    return detail::MessageDigestImpl<                                          \
        OpenSSLMethodPrefix##_Init, OpenSSLMethodPrefix##_Update,              \
        OpenSSLMethodPrefix##_Final, OpenSSLContextPrefix##_CTX,               \
        OpenSSLOutputSizePrefix##_DIGEST_LENGTH>(data);                        \
  }                                                                            \
  std::string Hmac##MethodName(std::string_view key, std::string_view data) {  \
    return detail::HmacImpl<OpenSSLOutputSizePrefix##_DIGEST_LENGTH>(          \
        EVP_##EVPSuffix(), key, data);                                         \
  }                                                                            \
  std::string Hmac##MethodName(std::string_view key,                           \
                               std::initializer_list<std::string_view> data) { \
    return detail::HmacImpl<OpenSSLOutputSizePrefix##_DIGEST_LENGTH>(          \
        EVP_##EVPSuffix(), key, data);                                         \
  }                                                                            \
  std::string Hmac##MethodName(std::string_view key,                           \
                               const NoncontiguousBuffer& data) {              \
    return detail::HmacImpl<OpenSSLOutputSizePrefix##_DIGEST_LENGTH>(          \
        EVP_##EVPSuffix(), key, data);                                         \
  }

}  // namespace flare::detail

#endif  // FLARE_BASE_CRYPTO_DETAIL_OPENSSL_IMPL_H_
