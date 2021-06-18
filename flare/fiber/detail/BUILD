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
  name = 'context',
  hdrs = [],
  srcs = glob(build_target.arch + '/*.S')
)

cc_benchmark(
  name = 'context_benchmark',
  srcs = 'context_benchmark.cc',
  deps = [
    ':context',
  ]
)

cc_library(
  name = 'runnable_entity',
  hdrs = 'runnable_entity.h',
  deps = [
    '//flare/base:casting',
  ]
)

cc_library(
  name = 'run_queue',
  hdrs = 'run_queue.h',
  srcs = 'run_queue.cc',
  deps = [
    ':assembly',
    ':runnable_entity',
    '//flare/base:align',
    '//flare/base:likely',
    '//flare/base:logging',
    '//flare/base/thread:spinlock',
  ]
)

cc_test(
  name = 'run_queue_test',
  srcs = 'run_queue_test.cc',
  deps = [
    ':run_queue',
    '//flare/base/thread:latch',
    '//flare/base:random',
  ]
)

cc_library(
  name = 'assembly',
  hdrs = 'assembly.h',
)

cc_benchmark(
  name = 'assembly_benchmark',
  srcs = 'assembly_benchmark.cc',
  deps = [
    ':assembly',
  ]
)

cc_library(
  name = 'fiber_impl',
  hdrs = [
    'fiber_desc.h',
    'fiber_entity.h',
    'fiber_worker.h',
    'scheduling_group.h',
    'timer_worker.h',
    'waitable.h',
  ],
  srcs = [
    'fiber_desc.cc',
    'fiber_entity.cc',
    'scheduling_group.cc',
    'timer_worker.cc',
    'fiber_worker.cc',
    'waitable.cc',
  ],
  deps = [
    ':assembly',
    ':context',
    ':run_queue',
    ':runnable_entity',
    ':stack_allocator',
    '//flare/base:align',
    '//flare/base:casting',
    '//flare/base:chrono',
    '//flare/base:deferred',
    '//flare/base:delayed_init',
    '//flare/base:erased_ptr',
    '//flare/base:exposed_var',
    '//flare/base:function',
    '//flare/base:id_alloc',
    '//flare/base:logging',
    '//flare/base:object_pool',
    '//flare/base:random',
    '//flare/base:ref_ptr',
    '//flare/base:string',
    '//flare/base:tsc',
    '//flare/base/internal:annotation',
    '//flare/base/internal:builtin_monitoring',
    '//flare/base/internal:doubly_linked_list',
    '//flare/base/object_pool:ref_counted',
    '//flare/base/thread:attribute',
    '//flare/base/thread:latch',
    '//flare/base/thread:out_of_duty_callback',
    '//flare/base/thread:spinlock',
    '//thirdparty/gflags:gflags',
  ],
  visibility = [
    '//flare/fiber/...',
    '//flare/net/internal:http_engine', # !!! FIXME.
  ],
)

cc_test(
  name = 'fiber_desc_test',
  srcs = 'fiber_desc_test.cc',
  deps = [
    ':fiber_impl',
  ]
)

cc_test(
  name = 'fiber_entity_test',
  srcs = 'fiber_entity_test.cc',
  deps = [
    ':fiber_impl',
  ]
)

cc_test(
  name = 'scheduling_group_test',
  srcs = 'scheduling_group_test.cc',
  deps = [
    ':fiber_impl',
    ':testing',
    '//flare/base:chrono',
    '//flare/base:random',
    '//flare/base:string',
    '//thirdparty/gflags:gflags',
  ],
  exclusive = True
)

cc_test(
  name = 'timer_worker_test',
  srcs = 'timer_worker_test.cc',
  deps = [
    ':fiber_impl',
    '//flare/base:chrono',
    '//flare/base:random',
  ],
)

cc_test(
  name = 'fiber_worker_test',
  srcs = 'fiber_worker_test.cc',
  deps = [
    ':fiber_impl',
    '//flare/base:random',
    '//flare/base/internal:cpu',
    '//flare/fiber/detail:testing',
    '//thirdparty/gflags:gflags',
  ],
)

cc_test(
  name = 'waitable_test',
  srcs = 'waitable_test.cc',
  deps = [
    ':fiber_impl',
    '//flare/base:random',
    '//flare/fiber/detail:testing',
  ]
)

cc_library(
  name = 'stack_allocator',
  hdrs = 'stack_allocator.h',
  srcs = 'stack_allocator.cc',
  deps = [
    '//flare/base:logging',
    '//flare/base:object_pool',
    '//flare/base/internal:annotation',
    '//thirdparty/gflags:gflags',
  ]
)

cc_test(
  name = 'stack_allocator_test',
  srcs = 'stack_allocator_test.cc',
  deps = [
    ':stack_allocator',
    '//flare/base:logging',
    '//thirdparty/gflags:gflags',
  ],
  # Death test in this UT does not get along well with heap checker.
  heap_check = ''
)

cc_library(
  name = 'scheduling_parameters',
  hdrs = 'scheduling_parameters.h',
  srcs = 'scheduling_parameters.cc',
  deps = [
    '//flare/base:enum',
    '//flare/base:logging',
    '//flare/base/internal:cpu',
  ],
  visibility = ['//flare/fiber:...'],
)

cc_test(
  name = 'scheduling_parameters_test',
  srcs = 'scheduling_parameters_test.cc',
  deps = [
    ':scheduling_parameters',
    '//flare/base:logging',
  ]
)

# For internal use only. For non-flare developers, use `flare/testing/main.h`
# instead.
cc_library(
  name = 'testing',
  hdrs = 'testing.h',
  deps = [
    ':fiber_impl',
    '//flare/fiber:fiber',
  ],
  visibility = ['//flare/fiber/...']
)
