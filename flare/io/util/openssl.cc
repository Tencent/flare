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

namespace flare::io::util {

// OpenSSL 1.1+ (flare now links 3.x via vcpkg) auto-initializes the library,
// error strings and algorithms on first use and is internally thread-safe. The
// legacy OpenSSL 1.0.x ceremony this file used to perform -- SSL_library_init,
// ERR_load_*/SSL_load_error_strings, ENGINE_load_builtin_engines, and the
// CRYPTO_set_locking_callback / CRYPTO_num_locks thread-locking shims -- is
// obsolete (deprecated no-ops; CRYPTO_num_locks() returns 0). Both entry points
// are kept (so callers need no change) but are now empty.

void InitializeOpenSSL() {}

void DestroyOpenSSL() {}

}  // namespace flare::io::util
