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
  name = 'fiber',
  hdrs = [
    'async.h',
    'alternatives.h',
    'condition_variable.h',
    'execution_context.h',
    'fiber_local.h',
    'fiber.h',
    'future.h',
    'latch.h',
    'logging.h',
    'mutex.h',
    'runtime.h',
    'semaphore.h',
    'shared_mutex.h',
    'this_fiber.h',
    'timer.h',
    'work_queue.h',
  ],
  deps = [
    ':alternatives',
    ':async',
    ':fiber_basic',
    ':future',
    ':logging',
    ':timer',
    ':work_queue',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'async',
  hdrs = 'async.h',
  deps = [
    ':fiber_basic',
    '//flare/base:future',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'timer',
  hdrs = 'timer.h',
  srcs = 'timer.cc',
  deps = [
    ':fiber_basic',
    '//flare/base:function',
    '//flare/fiber/detail:fiber_impl',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'future',
  hdrs = 'future.h',
  deps = [
    ':fiber_basic',
    '//flare/base:delayed_init',
    '//flare/base:future',
    '//flare/base/internal:time_view',
    '//flare/fiber/detail:fiber_impl',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'logging',
  hdrs = 'logging.h',
  srcs = 'logging.cc',
  deps = [
    ':fiber_basic',
    '//flare/base:string',
    '//flare/base/internal:logging',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'alternatives',
  hdrs = 'alternatives.h',
  srcs = 'alternatives.cc',
  visibility = 'PUBLIC',
)

#############################################
# TARGET BELOW ARE FOR INTERNAL USE.        #
#                                           #
# YOU SHOULD USE `fiber` ABOVE INSTEAD.     #
#############################################

cc_library(
  name = 'fiber_basic',
  hdrs = [
    'condition_variable.h',
    'execution_context.h',
    'fiber_local.h',
    'fiber.h',
    'latch.h',
    'mutex.h',
    'runtime.h',
    'semaphore.h',
    'shared_mutex.h',
    'this_fiber.h',
  ],
  srcs = [
    'condition_variable.cc',
    'execution_context.cc',
    'fiber.cc',
    'latch.cc',
    'mutex.cc',
    'runtime.cc',
    'shared_mutex.cc',
    'this_fiber.cc',
  ],
  deps = [
    '//flare/base:chrono',
    '//flare/base:deferred',
    '//flare/base:function',
    '//flare/base:likely',
    '//flare/base:logging',
    '//flare/base:random',
    '//flare/base:ref_ptr',
    '//flare/base:string',
    '//flare/base/internal:annotation',
    '//flare/base/internal:cpu',
    '//flare/base/internal:index_alloc',
    '//flare/base/object_pool:ref_counted',
    '//flare/base/thread:attribute',
    '//flare/base/thread:semaphore',
    '//flare/fiber/detail:fiber_impl',
    '//flare/fiber/detail:scheduling_parameters',
    '//thirdparty/gflags:gflags',
  ],
  visibility = [
    ':fiber',
  ],
)

cc_test(
  name = 'fiber_test',
  srcs = 'fiber_test.cc',
  deps = [
    ':fiber',
    '//flare/base:random',
    '//flare/base/internal:cpu',
    '//thirdparty/gflags:gflags',
  ],
)

cc_test(
  name = 'condition_variable_test',
  srcs = 'condition_variable_test.cc',
  deps = [
    ':fiber',
    '//flare/base:random',
    '//flare/fiber/detail:testing',
  ],
)

cc_test(
  name = 'fiber_local_test',
  srcs = 'fiber_local_test.cc',
  deps = [
    ':fiber',
    '//flare/base:random',
    '//flare/fiber/detail:testing',
    '//thirdparty/googletest:gmock',
  ]
)

cc_benchmark(
  name = 'fiber_local_benchmark',
  srcs = 'fiber_local_benchmark.cc',
  deps = [
    ':fiber',
    '//flare:init',
  ]
)

cc_test(
  name = 'latch_test',
  srcs = 'latch_test.cc',
  deps = [
    ':fiber',
    '//flare/base:chrono',
    '//flare/fiber/detail:testing',
  ]
)

cc_test(
  name = 'this_fiber_test',
  srcs = 'this_fiber_test.cc',
  deps = [
    ':fiber',
    '//flare/base:random',
    '//flare/fiber/detail:testing',
    '//thirdparty/gflags:gflags',
  ],
  exclusive = True
)

cc_test(
  name = 'execution_context_test',
  srcs = 'execution_context_test.cc',
  deps = [
    ':async',
    ':fiber',
    '//flare/testing:main',
  ]
)

cc_benchmark(
  name = 'execution_context_benchmark',
  srcs = 'execution_context_benchmark.cc',
  deps = [
    ':fiber',
    '//flare:init',
  ]
)

cc_test(
  name = 'async_test',
  srcs = 'async_test.cc',
  deps = [
    ':fiber',
    '//flare/fiber/detail:testing',
  ]
)

cc_library(
  name = 'work_queue',
  hdrs = 'work_queue.h',
  srcs = 'work_queue.cc',
  deps = [
    ':fiber_basic',
    '//flare/base:function',
  ],
  visibility = [
    ':fiber',
    '//flare/rpc/internal:stream_io_adaptor',
  ],
)

cc_test(
  name = 'work_queue_test',
  srcs = 'work_queue_test.cc',
  deps = [
    ':fiber',
    ':work_queue',
    '//flare/fiber/detail:testing',
  ]
)

cc_test(
  name = 'timer_test',
  srcs = 'timer_test.cc',
  deps = [
    ':timer',
    ':fiber',
    '//flare/fiber/detail:testing',
  ],
  exclusive = True,
)

cc_test(
  name = 'future_test',
  srcs = 'future_test.cc',
  deps = [
    ':fiber',
    ':future',
    '//flare/base:random',
    '//flare/fiber/detail:testing',
    '//thirdparty/googletest:gmock',
  ]
)

cc_test(
  name = 'logging_test',
  srcs = 'logging_test.cc',
  deps = [
    ':fiber',
    '//flare/base:logging',
    '//flare/fiber:logging',
    '//flare/testing:main',
  ]
)

cc_test(
  name = 'semaphore_test',
  srcs = 'semaphore_test.cc',
  deps = [
    ':fiber_basic',
    '//flare/fiber/detail:testing',
  ]
)

cc_test(
  name = 'shared_mutex_test',
  srcs = 'shared_mutex_test.cc',
  deps = [
    ':fiber_basic',
    '//flare/base:random',
    '//flare/fiber/detail:testing',
  ],
)

cc_test(
  name = 'runtime_test',
  srcs = 'runtime_test.cc',
  deps = [
    ':fiber',
    '//flare/base/thread:latch',
    '//flare/base/internal:cpu',
    '//flare/base/thread:attribute',
    '//flare/testing:main',
    '//thirdparty/gflags:gflags',
  ]
)

cc_test(
  name = 'alternatives_test',
  srcs = 'alternatives_test.cc',
  deps = [
    ':fiber',
    '//flare/fiber/detail:testing',
  ]
)
