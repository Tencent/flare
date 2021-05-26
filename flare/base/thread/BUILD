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
  name = 'out_of_duty_callback',
  hdrs = 'out_of_duty_callback.h',
  srcs = 'out_of_duty_callback.cc',
  deps = [
    '//flare/base:chrono',
    '//flare/base:function',
    '//flare/base:never_destroyed',
    '//flare/base/thread:thread_local',
    '//flare/base/internal:biased_mutex',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'out_of_duty_callback_test',
  srcs = 'out_of_duty_callback_test.cc',
  deps = [
    ':out_of_duty_callback'
  ]
)

cc_benchmark(
  name = 'out_of_duty_callback_benchmark',
  srcs = 'out_of_duty_callback_benchmark.cc',
  deps = [
    ':out_of_duty_callback',
  ]
)

cc_library(
  name = 'attribute',
  hdrs = 'attribute.h',
  srcs = 'attribute.cc',
  deps = [
    '//flare/base:logging',
    '#pthread',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'attribute_test',
  srcs = 'attribute_test.cc',
  deps = [
    ':attribute',
    '//flare/base:logging',
    '//flare/base:random',
    '//flare/base/internal:cpu',
  ]
)

cc_library(
  name = 'thread_cached',
  hdrs = 'thread_cached.h',
  deps = [
    '//flare/base/thread:thread_local',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'thread_cached_test',
  srcs = 'thread_cached_test.cc',
  deps = [
    ':thread_cached',
    '//flare/base:chrono',
    '//flare/base:string',
    '//flare/base:random',
    '//flare/base/thread:latch',
  ]
)

cc_benchmark(
  name = 'thread_cached_benchmark',
  srcs = 'thread_cached_benchmark.cc',
  deps = ':thread_cached',
)

cc_library(
  name = 'semaphore',
  hdrs = 'semaphore.h',
  visibility = 'PUBLIC',
)

cc_test(
  name = 'semaphore_test',
  srcs = 'semaphore_test.cc',
  deps = [
    ':semaphore',
  ]
)

cc_library(
  name = 'latch',
  hdrs = 'latch.h',
  srcs = 'latch.cc',
  deps = [
    '//flare/base/internal:logging',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'latch_test',
  srcs = 'latch_test.cc',
  deps = [
    ':latch',
    '//flare/base:chrono',
  ]
)

cc_library(
  name = 'thread_local',
  hdrs = 'thread_local.h',
  deps = [
    '//flare/base:function',
    '//flare/base:likely',
    '//flare/base:logging',
    '//flare/base/thread/thread_local:always_initialized',
    '//flare/base/thread/thread_local/detail:atomic_ptr',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'thread_local_test',
  srcs = 'thread_local_test.cc',
  deps = [
    ':thread_local',
    ':latch',
  ]
)

cc_benchmark(
  name = 'thread_local_benchmark',
  srcs = 'thread_local_benchmark.cc',
  deps = [
    ':thread_local',
    # Tests workaround for gperftools's false-sharing issue.
    '//thirdparty/gperftools:tcmalloc_minimal',
  ]
)

cc_library(
  name = 'spinlock',
  hdrs = 'spinlock.h',
  srcs = 'spinlock.cc',
  deps = [
    '//flare/base:likely',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'spinlock_test',
  srcs = 'spinlock_test.cc',
  deps = [
    ':spinlock',
    ':latch',
  ]
)
