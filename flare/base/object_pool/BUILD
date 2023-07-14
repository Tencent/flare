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
  name = 'types',
  hdrs = 'types.h',
  deps = [
    '//flare/base:logging',
    '//flare/base:type_index',
    '//flare/base/internal:meta',
  ],
  visibility = ['//flare/base:object_pool'],
)

cc_library(
  name = 'ref_counted',
  hdrs = 'ref_counted.h',
  deps = [
    '//flare/base:ref_ptr',
    '//flare/base:object_pool',
  ],
  visibility = 'PUBLIC'
)

cc_test(
  name = 'ref_counted_test',
  srcs = 'ref_counted_test.cc',
  deps = [
    ':ref_counted',
    '//flare/testing:main',
  ]
)

cc_library(
  name = 'disabled',
  hdrs = 'disabled.h',
  srcs = 'disabled.cc',
  deps = [
    ':types',
  ],
  visibility = ['//flare/base:object_pool'],
)

cc_test(
  name = 'disabled_test',
  srcs = 'disabled_test.cc',
  deps = [
    ':disabled',
    '//flare/base:object_pool',
  ]
)

cc_library(
  name = 'thread_local',
  hdrs = 'thread_local.h',
  srcs = 'thread_local.cc',
  deps = [
    ':types',
    '//flare/base:chrono',
    '//flare/base:deferred',
    '//flare/base:demangle',
    '//flare/base:erased_ptr',
    '//flare/base:logging',
    '//flare/base/internal:annotation',
  ],
  visibility = ['//flare/base:object_pool'],
)

cc_test(
  name = 'thread_local_test',
  srcs = 'thread_local_test.cc',
  deps = [
    ':thread_local',
    '//flare/base:object_pool',
  ]
)

cc_library(
  name = 'memory_node_shared',
  hdrs = 'memory_node_shared.h',
  srcs = 'memory_node_shared.cc',
  deps = [
    ':types',
    '//flare/base:align',
    '//flare/base:chrono',
    '//flare/base:deferred',
    '//flare/base:demangle',
    '//flare/base:erased_ptr',
    '//flare/base:exposed_var',
    '//flare/base:likely',
    '//flare/base:logging',
    '//flare/base:never_destroyed',
    '//flare/base/thread:spinlock',
    '//flare/base/internal:annotation',
    '//flare/base/internal:background_task_host',
    '//flare/base/internal:cpu',
    '//flare/base/internal:doubly_linked_list',
    '//flare/base/internal:time_keeper',
  ],
  visibility = ['//flare/base:object_pool'],
)

cc_test(
  name = 'memory_node_shared_test',
  srcs = 'memory_node_shared_test.cc',
  deps = [
    '//flare/base:object_pool',
    '//flare/base/thread:latch',
    '//flare/base:random',
  ]
)

cc_benchmark(
  name = 'memory_node_shared_benchmark',
  srcs = 'memory_node_shared_benchmark.cc',
  deps = [
    '//flare/base:object_pool',
  ]
)

cc_library(
  name = 'global',
  hdrs = 'global.h',
  srcs = 'global.cc',
  deps = [
    ':types',
  ],
  visibility = ['//flare/base:object_pool'],
)

cc_test(
  name = 'global_test',
  srcs = 'global_test.cc',
  deps = [
    '//flare/base:object_pool',
  ]
)
