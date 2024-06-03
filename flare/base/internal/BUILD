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
  name = 'annotation',
  hdrs = 'annotation.h',
  deps = [
    ':logging'
  ],
  visibility = ['//flare/...'],
)

cc_library(
  name = 'background_task_host',
  hdrs = 'background_task_host.h',
  srcs = 'background_task_host.cc',
  deps = [
    ':cpu',
    ':thread_pool',
    '//flare/base:function',
    '//flare/base:never_destroyed',
    '//flare/base:random',
    '//flare/base/internal:logging',
    '//flare/base/thread:attribute',
    '//thirdparty/gflags:gflags',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'background_task_host_test',
  srcs = 'background_task_host_test.cc',
  deps = [
    ':background_task_host',
    '//flare/base/thread:latch',
  ]
)

cc_library(
  name = 'circular_buffer',
  hdrs = 'circular_buffer.h',
  deps = [
    '//flare/base:likely',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'circular_buffer_test',
  srcs = 'circular_buffer_test.cc',
  deps = [
    ':circular_buffer',
  ]
)

cc_benchmark(
  name = 'circular_buffer_benchmark',
  srcs = 'circular_buffer_benchmark.cc',
  deps = ':circular_buffer',
)

cc_library(
  name = 'dpc',
  hdrs = 'dpc.h',
  srcs = 'dpc.cc',
  deps = [
    ':background_task_host',
    ':cpu',
    ':time_keeper',
    '//flare/base:chrono',
    '//flare/base:function',
    '//flare/base:object_pool',
    '//flare/base/thread/thread_local:ref_counted',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'dpc_test',
  srcs = 'dpc_test.cc',
  deps = [
    ':dpc',
    '//flare/base:write_mostly',
    '//flare/fiber:fiber',
    '//flare/testing:main',
  ]
)

cc_benchmark(
  name = 'dpc_benchmark',
  srcs = 'dpc_benchmark.cc',
  deps = [
    ':dpc',
    '//flare:init',
    '//flare/base/internal:background_task_host',
  ]
)

cc_library(
  name = 'lazy_init',
  hdrs = 'lazy_init.h',
  deps = [
    '//flare/base:never_destroyed',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'lazy_init_test',
  srcs = 'lazy_init_test.cc',
  deps = [':lazy_init']
)

cc_library(
  name = 'early_init',
  hdrs = 'early_init.h',
  deps = ['//flare/base:never_destroyed'],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'early_init_test',
  srcs = 'early_init_test.cc',
  deps = [':early_init']
)

cc_library(
  name = 'doubly_linked_list',
  hdrs = 'doubly_linked_list.h',
  deps = [
    ':logging',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'doubly_linked_list_test',
  srcs = 'doubly_linked_list_test.cc',
  deps = [':doubly_linked_list']
)

cc_library(
  name = 'singly_linked_list',
  hdrs = 'singly_linked_list.h',
  deps = [
    ':logging',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'singly_linked_list_test',
  srcs = 'singly_linked_list_test.cc',
  deps = [':singly_linked_list']
)

cc_library(
  name = 'index_alloc',
  hdrs = 'index_alloc.h',
  srcs = 'index_alloc.cc',
  deps = [
    '//flare/base:never_destroyed',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'index_alloc_test',
  srcs = 'index_alloc_test.cc',
  deps = [':index_alloc']
)

cc_library(
  name = 'cpu',
  hdrs = 'cpu.h',
  srcs = 'cpu.cc',
  deps = [
    ':logging',
    '//flare/base:string',
    '//flare/base/thread:attribute',
    '//thirdparty/gflags:gflags',
    '//thirdparty/glog:glog',
    '#dl',
  ],
  visibility = [
    '//flare/...',
    '//yadcc/...',  # FIXME: Move `cpu` out to `flare/base`.
  ]
)

cc_test(
  name = 'cpu_test',
  srcs = 'cpu_test.cc',
  deps = [
    ':cpu',
    '//thirdparty/googletest:gmock',
  ]
)

cc_library(
  name = 'curl',
  hdrs = 'curl.h',
  srcs = 'curl.cc',
  deps = [
    ':logging',
    '//flare/base:deferred',
    '//flare/base:expected',
    '//flare/base:function',
    '//flare/base:logging',
    '//flare/base:never_destroyed',
    '//flare/base/thread:attribute',
    '//thirdparty/curl:curl',
    '//thirdparty/gflags:gflags',
    '#dl',
  ],
  visibility = ['//flare/...'],
)

cc_library(
  name = 'memory_barrier',
  hdrs = 'memory_barrier.h',
  srcs = 'memory_barrier.cc',
  deps = [
    ':logging',
    '//flare/base:never_destroyed',
  ],
  visibility = ['//flare/...'],
)

cc_benchmark(
  name = 'memory_barrier_benchmark',
  srcs = 'memory_barrier_benchmark.cc',
  deps = ':memory_barrier'
)

cc_library(
  name = 'time_view',
  hdrs = 'time_view.h',
  deps = [
    ':meta',
    '//flare/base:chrono',
  ],
  visibility = 'PUBLIC',  # FIXME: Move this out to `flare/base`
)

cc_test(
  name = 'time_view_test',
  srcs = 'time_view_test.cc',
  deps = ':time_view',
  exclusive = True,
)

cc_library(
  name = 'time_keeper',
  hdrs = 'time_keeper.h',
  srcs = 'time_keeper.cc',
  deps = [
    ':background_task_host',
    '//flare/base:function',
    '//flare/base:never_destroyed',
    '//flare/base:ref_ptr',
    '#pthread',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'time_keeper_test',
  srcs = 'time_keeper_test.cc',
  deps = [
    ':time_keeper',
    '//flare/base:chrono',
    '//flare/testing:main',
  ],
  exclusive = True
)

cc_library(
  name = 'logging',
  hdrs = 'logging.h',
  srcs = 'logging.cc',
  deps = [
    '//flare/base:likely',
    '//flare/base/internal:macro',
    '//thirdparty/fmt:fmt',
    '//thirdparty/glog:glog',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'logging_test',
  srcs = 'logging_test.cc',
  deps = [
    ':logging',
    '//thirdparty/googletest:gmock',
  ]
)

cc_library(
  name = 'biased_mutex',
  hdrs = 'biased_mutex.h',
  deps = [
    ':annotation',
    ':memory_barrier',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'biased_mutex_test',
  srcs = 'biased_mutex_test.cc',
  deps = [
    ':biased_mutex',
    '//flare/base:chrono',
  ]
)

cc_benchmark(
  name = 'biased_mutex_benchmark',
  srcs = 'biased_mutex_benchmark.cc',
  deps = ':biased_mutex'
)

cc_library(
  name = 'thread_pool',
  hdrs = 'thread_pool.h',
  srcs = 'thread_pool.cc',
  deps = [
    ':cpu',
    ':logging',
    '//flare/base:function',
    '//flare/base/thread:attribute',
    '#pthread',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'thread_pool_test',
  srcs = 'thread_pool_test.cc',
  deps = [
    ':thread_pool',
  ]
)

cc_library(
  name = 'case_insensitive_hash_map',
  hdrs = 'case_insensitive_hash_map.h',
  deps = [
    ':hash_map',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'case_insensitive_hash_map_test',
  srcs = 'case_insensitive_hash_map_test.cc',
  deps = ':case_insensitive_hash_map',
)

cc_benchmark(
  name = 'case_insensitive_hash_map_benchmark',
  srcs = 'case_insensitive_hash_map_benchmark.cc',
  deps = [
    '//flare/base:random',
    ':case_insensitive_hash_map',
  ]
)

cc_library(
  name = 'hash_map',
  hdrs = 'hash_map.h',
  deps = [
    ':logging',
    ':meta',
    '//flare/base:logging',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'hash_map_test',
  srcs = 'hash_map_test.cc',
  deps = [
    ':hash_map',
    '//flare/base:random',
  ]
)

cc_benchmark(
  name = 'hash_map_benchmark',
  srcs = 'hash_map_benchmark.cc',
  deps = ':hash_map'
)

cc_library(
  name = 'macro',
  hdrs = 'macro.h',
  visibility = ['//flare/...'],
)

cc_library(
  name = 'builtin_monitoring',
  hdrs = 'builtin_monitoring.h',
  deps = [
    '//flare/base:delayed_init',
    '//flare/base:monitoring',
    '//flare/base/monitoring:init',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'builtin_monitoring_test',
  srcs = 'builtin_monitoring_test.cc',
  deps = [
    ':builtin_monitoring',
    '//flare/base:down_cast',
    '//flare/base/monitoring:monitoring_system',
    '//flare/init:override_flag',
    '//flare/testing:main',
  ],
  testdata = [
    ('testdata', 'conf')
  ]
)

cc_library(
  name = 'copyable_atomic',
  hdrs = 'copyable_atomic.h',
  visibility = ['//flare/...'],
)

cc_test(
  name = 'copyable_atomic_test',
  srcs = 'copyable_atomic_test.cc',
  deps = [
    ':copyable_atomic'
  ]
)

cc_library(
  name = 'meta',
  hdrs = 'meta.h',
  visibility = ['//flare/...'],
)

cc_library(
  name = 'test_prod',
  hdrs = 'test_prod.h',
  visibility = ['//flare/...'],
)
