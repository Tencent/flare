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
  name = 'uuid',
  hdrs = 'uuid.h',
  srcs = 'uuid.cc',
  deps = [
    '//flare/base:string',
    '//flare/base:logging',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'uuid_test',
  srcs = 'uuid_test.cc',
  deps = [
    ':uuid',
    '//flare/base:string',
  ]
)

cc_library(
  name = 'lazy_eval',
  hdrs = 'lazy_eval.h',
  deps = [
    '//flare/base:logging',
    '//flare/base:function',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'lazy_eval_test',
  srcs = 'lazy_eval_test.cc',
  deps = [
    ':lazy_eval',
  ]
)

cc_library(
  name = 'bloom_filter',
  hdrs = 'bloom_filter.h',
  srcs = 'bloom_filter.cc',
  deps = [
    '//flare/base:logging',
    '//flare/base:random',
    '//thirdparty/xxhash:xxhash',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'bloom_filter_test',
  srcs = 'bloom_filter_test.cc',
  deps = [
    ':bloom_filter',
    '//flare/base:logging',
  ]
)

cc_library(
  name = 'seqlocked',
  hdrs = 'seqlocked.h',
  deps = [
    '//flare/base:likely',
    '//flare/base/internal:memory_barrier',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'seqlocked_test',
  srcs = 'seqlocked_test.cc',
  deps = [
    ':seqlocked',
    '//flare/base:chrono',
    '//flare/base:random',
  ]
)

cc_benchmark(
  name = 'seqlocked_benchmark',
  srcs = 'seqlocked_benchmark.cc',
  deps = [
    ':seqlocked',
  ]
)

cc_library(
  name = 'byte_set',
  hdrs = 'byte_set.h',
  srcs = 'byte_set.cc',
  visibility = 'PUBLIC',
)

cc_test(
  name = 'byte_set_test',
  srcs = 'byte_set_test.cc',
  deps = [
    ':byte_set',
  ]
)

cc_library(
  name = 'flyweight',
  hdrs = 'flyweight.h',
  deps = [
    '//flare/base:never_destroyed',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'flyweight_test',
  srcs = 'flyweight_test.cc',
  deps = [
    ':flyweight',
  ]
)
