# Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
#
# Licensed under the BSD 3-Clause License (the "License"); you may not use this
# file except in compliance with the License. You may obtain a copy of the
# License at
#
# https://opensource.org/licenses/BSD-3-Clause
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

cc_library(
  name = 'blake3',
  hdrs = 'blake3.h',
  srcs = 'blake3.cc',
  deps = [
    '//flare/base:buffer',
    '//thirdparty/blake3:blake3',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'blake3_test',
  srcs = 'blake3_test.cc',
  deps = [
    ':blake3',
    '//flare/base:buffer',
    '//flare/base:encoding',
  ]
)

cc_library(
  name = 'sha',
  hdrs = 'sha.h',
  srcs = 'sha.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base/crypto/detail:openssl_impl',
    '//thirdparty/openssl:crypto',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'sha_test',
  srcs = 'sha_test.cc',
  deps = [
    ':sha',
    '//flare/base:buffer',
    '//flare/base:encoding',
  ]
)


cc_library(
  name = 'md5',
  hdrs = 'md5.h',
  srcs = 'md5.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base/crypto/detail:openssl_impl',
    '//thirdparty/openssl:crypto',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'md5_test',
  srcs = 'md5_test.cc',
  deps = [
    ':md5',
    '//flare/base:buffer',
    '//flare/base:encoding',
  ]
)
