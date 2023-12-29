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
  name = 'load_balancer',
  hdrs = 'load_balancer.h',
  srcs = 'load_balancer.cc',
  deps = [
    '//flare/base:dependency_registry',
    '//flare/base/net:endpoint',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'consistent_hash',
  hdrs = 'consistent_hash.h',
  srcs = 'consistent_hash.cc',
  deps = [
    ':load_balancer',
    '//flare/base:hazptr',
    '//flare/base:string',
    '//flare/io:io_basic',
  ],
  link_all_symbols = True,
  visibility = 'PUBLIC',
)

cc_test(
  name = 'consistent_hash_test',
  srcs = ['consistent_hash_test.cc'],
  deps = [
      ':consistent_hash',
      '//flare/base:random',
      '//flare/base:string',
      '//flare/base/net:endpoint',
  ],
)

cc_library(
  name = 'round_robin',
  hdrs = 'round_robin.h',
  srcs = 'round_robin.cc',
  deps = [
    ':load_balancer',
    '//flare/base:hazptr',
    '//flare/base:random',
    '//flare/io:io_basic',
  ],
  link_all_symbols = True,
  visibility = 'PUBLIC',
)

cc_test(
  name = 'round_robin_test',
  srcs = [],
  deps = []
)
