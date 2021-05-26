# Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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
  name = 'override_flag',
  hdrs = 'override_flag.h',
  srcs = 'override_flag.cc',
  deps = [
    '//flare/base:logging',
    '//flare/base:never_destroyed',
    '//flare/base/internal:macro',
    '//thirdparty/gflags:gflags',
    '//thirdparty/glog:glog',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'override_flag_test',
  srcs = 'override_flag_test.cc',
  deps = [
    ':override_flag',
    '//flare/testing:main',
    '//thirdparty/gflags:gflags',
  ]
)

cc_library(
  name = 'on_init',
  hdrs = 'on_init.h',
  srcs = 'on_init.cc',
  deps = [
    '//flare/base:function',
    '//flare/base:logging',
    '//flare/base:never_destroyed',
    '//flare/base:random',
    '//flare/base/internal:macro',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'on_init_test',
  srcs = 'on_init_test.cc',
  deps = [
    ':on_init',
    '//flare/testing:main',
  ]
)

cc_library(
  name = 'openssl_initializer',
  hdrs = [],
  srcs = 'openssl_initializer.cc',
  deps = [
    ':on_init',
    '//flare/io/util:openssl'
  ],
  link_all_symbols = True,
  visibility = 'PUBLIC',
)

cc_library(
  name = 'override_rlimit',
  hdrs = [],
  srcs = 'override_rlimit.cc',
  deps = [
    ':on_init',
    '//flare/base:logging',
    '//flare/base:string',
    '//thirdparty/gflags:gflags',
  ],
  link_all_symbols = True,
  visibility = 'PUBLIC',
)

cc_test(
  name = 'override_rlimit_test',
  srcs = 'override_rlimit_test.cc',
  deps = [
    ':override_rlimit',
    ':override_flag',
    '//flare/base:logging',
    '//flare/testing:main',
  ]
)
