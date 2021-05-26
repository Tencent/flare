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
  name = 'redis',
  hdrs = [
    'redis_channel.h',
    'redis_client.h',
    'redis_command.h',
    'redis_object.h',
  ],
  deps = [
    ':redis_channel',
    ':redis_client',
    ':redis_command',
    ':redis_object',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'redis_object',
  hdrs = 'redis_object.h',
  deps = [
    '//flare/base:buffer',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'reader',
  hdrs = 'reader.h',
  srcs = 'reader.cc',
  deps = [
    ':redis_object',
    '//flare/base:buffer',
    '//flare/base:string',
  ],
  visibility = ['//flare/testing:redis_mock'],
)

cc_test(
  name = 'reader_test',
  srcs = 'reader_test.cc',
  deps = [
    ':reader',
    '//flare/base:buffer',
  ]
)

cc_library(
  name = 'redis_command',
  hdrs = 'redis_command.h',
  srcs = 'redis_command.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base:string',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'redis_command_test',
  srcs = 'redis_command_test.cc',
  deps = [
    ':redis_command',
    '//flare/base:buffer',
  ]
)

cc_library(
  name = 'message',
  hdrs = 'message.h',
  deps = [
    ':redis_command',
    ':redis_object',
    '//flare/rpc/protocol:message',
  ]
)

cc_library(
  name = 'redis_protocol',
  hdrs = 'redis_protocol.h',
  srcs = 'redis_protocol.cc',
  deps = [
    ':message',
    ':reader',
    ':redis_command',
    ':redis_object',
    '//flare/base:buffer',
    '//flare/base:casting',
    '//flare/rpc/protocol:stream_protocol',
  ]
)

cc_test(
  name = 'redis_protocol_test',
  srcs = 'redis_protocol_test.cc',
  deps = [
    ':message',
    ':redis_protocol',
    '//flare/base:down_cast',
  ]
)

cc_library(
  name = 'mock_channel',
  hdrs = 'mock_channel.h',
  deps = [
    ':redis_command',
    ':redis_object',
    '//flare/base:function',
  ],
  visibility = ['//flare/testing:redis_mock'],
)

cc_library(
  name = 'redis_channel',
  hdrs = 'redis_channel.h',
  srcs = 'redis_channel.cc',
  deps = [
    ':message',
    ':mock_channel',
    ':redis_command',
    ':redis_object',
    ':redis_protocol',
    '//flare/base:casting',
    '//flare/base:function',
    '//flare/base:string',
    '//flare/base/encoding:hex',
    '//flare/base/internal:early_init',
    '//flare/base/internal:lazy_init',
    '//flare/rpc/internal:stream_call_gate_pool',
    '//flare/rpc/internal:stream_call_gate',
    '//flare/rpc/load_balancer:round_robin',
    '//flare/rpc/message_dispatcher:composited',
    '//flare/rpc/name_resolver:list',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'redis_client',
  hdrs = 'redis_client.h',
  srcs = 'redis_client.cc',
  deps = [
    ':redis_channel',
    ':redis_command',
    ':redis_object',
    '//flare/base:future',
    '//flare/base:maybe_owning',
    '//flare/base:string',
    '//flare/base/internal:early_init',
    '//flare/base/internal:lazy_init',
    '//flare/base/internal:time_view',
    '//flare/fiber:fiber',
    '//thirdparty/gflags:gflags',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'redis_test',
  srcs = 'redis_test.cc',
  deps = [
    ':redis',
    '//flare/base:string',
    '//flare/fiber:fiber',
    '//flare/testing:endpoint',
    '//flare/testing:main',
    '//flare/testing:naked_server',
  ]
)
