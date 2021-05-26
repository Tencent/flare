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

#include "flare/io/util/openssl.h"

#include <pthread.h>
#include <stdint.h>

#include "thirdparty/openssl/crypto.h"
#include "thirdparty/openssl/engine.h"
#include "thirdparty/openssl/err.h"
#include "thirdparty/openssl/ssl.h"

namespace flare::io::util {

pthread_mutex_t* ssl_lock = nullptr;

void CallbackLockFunction(int32_t mode, int32_t type, const char* file,
                          int32_t line) {
  if (mode & CRYPTO_LOCK) {
    pthread_mutex_lock(&(ssl_lock[type]));
  } else {
    pthread_mutex_unlock(&(ssl_lock[type]));
  }
}

unsigned long CallbackIdFunction() {
  return static_cast<unsigned long>(pthread_self());
}

void InitializeOpenSSL() {
  ERR_load_ERR_strings();
  ERR_load_crypto_strings();
  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  ENGINE_load_builtin_engines();

  // when openssl is used in multi-thread environment.
  ssl_lock = reinterpret_cast<pthread_mutex_t*>(
      OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t)));

  uint32_t ssl_lock_num = CRYPTO_num_locks();
  for (int i = 0; i < ssl_lock_num; ++i) {
    pthread_mutex_init(&(ssl_lock[i]), nullptr);
  }

  CRYPTO_set_id_callback(CallbackIdFunction);
  CRYPTO_set_locking_callback(CallbackLockFunction);
}

void DestroyOpenSSL() {
  if (!ssl_lock) return;

  ENGINE_cleanup();
  CRYPTO_set_locking_callback(nullptr);
  uint32_t ssl_lock_num = CRYPTO_num_locks();
  for (int i = 0; i < ssl_lock_num; ++i) {
    pthread_mutex_destroy(&(ssl_lock[i]));
  }
  OPENSSL_free(ssl_lock);
  ssl_lock = nullptr;
}

}  // namespace flare::io::util
