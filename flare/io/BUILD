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
  name = 'stream_connection',
  hdrs = 'stream_connection.h',
  deps = [
    '//flare/base:buffer',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'datagram_transceiver',
  hdrs = 'datagram_transceiver.h',
  deps = [
    '//flare/base:buffer',
    '//flare/base/net:endpoint',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'acceptor',
  hdrs = 'acceptor.h',
  visibility = 'PUBLIC',
)

cc_library(
  name = 'io_basic',
  hdrs = [
    'descriptor.h',
    'event_loop.h',
  ],
  srcs = [
    'descriptor.cc',
    'detail/event_loop_notifier.cc',
    'detail/event_loop_notifier.h',
    'detail/watchdog.cc',
    'detail/watchdog.h',
    'event_loop.cc',
  ],
  deps = [
    '//flare/base:align',
    '//flare/base:chrono',
    '//flare/base:deferred',
    '//flare/base:delayed_init',
    '//flare/base:enum',
    '//flare/base:exposed_var',
    '//flare/base:function',
    '//flare/base:handle',
    '//flare/base:likely',
    '//flare/base:logging',
    '//flare/base:maybe_owning',
    '//flare/base:random',
    '//flare/base:ref_ptr',
    '//flare/base:string',
    '//flare/base:tsc',
    '//flare/base/internal:memory_barrier',
    '//flare/base/internal:test_prod',
    '//flare/base/net:endpoint',
    '//flare/base/thread:latch',
    '//flare/fiber:fiber',
    '//flare/io/detail:eintr_safe',
    '//flare/io/detail:timed_call',
    '//flare/io/util:rate_limiter',
    '//flare/io/util:socket',
    '//flare/io/util:stream_io',
    '//thirdparty/gflags:gflags',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'descriptor_test',
  srcs = 'descriptor_test.cc',
  deps = [
    ':io_basic',
    '//flare/base:random',
    '//flare/fiber:fiber',
    '//flare/testing:main',
    '//flare/testing:endpoint',
  ]
)

cc_test(
  name = 'event_loop_test',
  srcs = ['event_loop_test.cc'],
  deps = [
    ':io_basic',
    '//flare/testing:main',
  ]
)
