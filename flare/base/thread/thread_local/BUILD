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
  name = 'always_initialized',
  hdrs = 'always_initialized.h',
  deps = [
    '//flare/base:function',
    '//flare/base:likely',
    '//flare/base:logging',
    '//flare/base/internal:memory_barrier',
  ],
  visibility = ['//flare/base/thread:thread_local'],
)

cc_library(
  name = 'ref_counted',
  hdrs = 'ref_counted.h',
  deps = [
    ':always_initialized',
    '//flare/base:function',
    '//flare/base:ref_ptr',
    '//flare/base/thread/thread_local/detail:atomic_ptr',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'ref_counted_test',
  srcs = 'ref_counted_test.cc',
  deps = ':ref_counted'
)

cc_benchmark(
  name = 'ref_counted_benchmark',
  srcs = 'ref_counted_benchmark.cc',
  deps = ':ref_counted',
)
