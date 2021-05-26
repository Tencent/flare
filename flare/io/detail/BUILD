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
  name = 'eintr_safe',
  hdrs = 'eintr_safe.h',
  srcs = 'eintr_safe.cc',
  deps = [
    '//flare/base:likely',
  ],
  visibility = ['//flare/...'],
)

cc_library(
  name = 'timed_call',
  hdrs = 'timed_call.h',
  srcs = 'timed_call.cc',
  deps = [
    '//flare/base:deferred',
    '//flare/base:logging',
    '//flare/base:tsc',
    '//thirdparty/gflags:gflags',
  ],
  visibility = ['//flare/...'],
)

cc_library(
  name = 'writing_buffer_list',
  hdrs = 'writing_buffer_list.h',
  srcs = 'writing_buffer_list.cc',
  deps = [
    ':eintr_safe',
    '//flare/base:align',
    '//flare/base:buffer',
    '//flare/base:chrono',
    '//flare/base:likely',
    '//flare/base:object_pool',
    '//flare/base/internal:annotation',
    '//flare/io/util:stream_io',
    '//thirdparty/googletest:gtest_prod',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'writing_buffer_list_test',
  srcs = 'writing_buffer_list_test.cc',
  deps = [
    ':writing_buffer_list',
    '//flare/base/thread:latch',
    '//flare/io/util:socket',
    '//flare/testing:main',
  ]
)

cc_library(
  name = 'writing_datagram_list',
  hdrs = 'writing_datagram_list.h',
  srcs = 'writing_datagram_list.cc',
  deps = [
    ':eintr_safe',
    '//flare/base:buffer',
    '//flare/base/internal:annotation',
    '//flare/base/net:endpoint',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'writing_datagram_list_test',
  srcs = 'writing_datagram_list_test.cc',
  deps = [
    ':writing_datagram_list',
    '//flare/io/util:socket',
    '//flare/testing:endpoint',
    '//flare/testing:main',
  ]
)

cc_library(
  name = 'read_at_most',
  hdrs = 'read_at_most.h',
  srcs = 'read_at_most.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base:logging',
    '//flare/base:object_pool',
    '//flare/base/internal:annotation',
    '//flare/io/util:stream_io',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'read_at_most_test',
  srcs = 'read_at_most_test.cc',
  deps = [
    ':read_at_most',
    '//flare/base:random',
    '//flare/io/util:socket',
    '//flare/testing:endpoint',
    '//flare/testing:main',
  ]
)

# `watchdog` is compiled with `io/event_loop.cc`. No `cc_library` here.

cc_test(
  name = 'watchdog_test',
  srcs = ['watchdog_test.cc', 'watchdog_test.h'],
  deps = [
    '//flare/io:io_basic',
    '//flare/testing:main',
    '//thirdparty/gflags:gflags',
  ]
)
