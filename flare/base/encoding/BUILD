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
  name = 'base64',
  hdrs = 'base64.h',
  srcs = 'base64.cc',
  deps = [
    '//flare/base:logging',
    '//flare/base/internal:early_init',
    '//thirdparty/openssl:crypto',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'base64_test',
  srcs = 'base64_test.cc',
  deps = [
    ':base64',
    '//flare/base:random',
  ],
)

cc_library(
  name = 'hex',
  hdrs = 'hex.h',
  srcs = 'hex.cc',
  deps = [
    '//flare/base:logging',
    '//flare/base/encoding/detail:hex_chars',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'hex_test',
  srcs = 'hex_test.cc',
  deps = ':hex'
)

cc_library(
  name = 'percent',
  hdrs = 'percent.h',
  srcs = 'percent.cc',
  deps = [
    '//flare/base:enum',
    '//flare/base:logging',
    '//flare/base/encoding/detail:hex_chars',
    '//flare/base/internal:early_init',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'percent_test',
  srcs = 'percent_test.cc',
  deps = [
    ':percent',
    '//flare/base:string',
  ]
)
