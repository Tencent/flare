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

load('//flare/tools/build_rules.bld', 'cc_flare_library')

cc_library(
  name = 'endpoint',
  hdrs = 'endpoint.h',
  srcs = 'endpoint.cc',
  deps = [
    '//flare/base:handle',
    '//flare/base:random',
    '//flare/base/net:endpoint',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'main',
  hdrs = 'main.h',
  srcs = 'main.cc',
  deps = [
    '//flare:init',
    '//thirdparty/gflags:gflags',
    '//thirdparty/glog:glog',
    '//thirdparty/googletest:gtest',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'hooking_mock',
  hdrs = 'hooking_mock.h',
  srcs = 'hooking_mock.cc',
  deps = [
    '//flare/base:logging',
    '//flare/base:never_destroyed',
    '//flare/base/internal:macro',
    '//flare/init:on_init',
    '//flare/testing/detail:dirty_hook',
    '//thirdparty/googletest:gmock',
    '//thirdparty/googletest:gtest',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'hooking_mock_test',
  srcs = 'hooking_mock_test.cc',
  deps = ':hooking_mock',
)

cc_library(
  name = 'rpc_mock',
  hdrs = 'rpc_mock.h',
  srcs = 'rpc_mock.cc',
  deps = [
    '//flare/base:callback',
    '//flare/base:down_cast',
    '//flare/base:status',
    '//flare/base:string',
    '//flare/base/internal:lazy_init',
    '//flare/init:on_init',
    '//flare/rpc:protobuf',
    '//flare/rpc/protocol/protobuf:mock_channel',
    '//flare/rpc/protocol/protobuf:protobuf',
    '//flare/testing/detail:gmock_actions',
    '//flare/testing/detail:implicitly_casting',
    '//thirdparty/googletest:gmock',
    '//thirdparty/googletest:gtest',
    '//thirdparty/protobuf:protobuf',
  ],
  link_all_symbols = True,
  visibility = 'PUBLIC',
)

cc_test(
  name = 'rpc_mock_test',
  srcs = 'rpc_mock_test.cc',
  deps = [
    ':echo_service_proto_flare',
    ':main',
    ':rpc_mock',
    '//flare/rpc:rpc',
  ]
)

cc_library(
  name = 'rpc_controller',
  hdrs = 'rpc_controller.h',
  srcs = 'rpc_controller.cc',
  deps = [
    '//flare/rpc:protobuf',
    '//flare/rpc/protocol/protobuf:rpc_client_controller',
    '//flare/rpc/protocol/protobuf:rpc_server_controller',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'hbase_controller',
  hdrs = 'hbase_controller.h',
  srcs = 'hbase_controller.cc',
  deps = [
    '//flare/net/hbase:hbase_client_controller',
    '//flare/net/hbase:hbase_server_controller',
  ],
  visibility = 'PUBLIC',
)

proto_library(
  name = 'message_proto',
  srcs = 'message.proto',
  visibility = 'PUBLIC',
)

proto_library(
  name = 'echo_service_proto',
  srcs = 'echo_service.proto',
  deps = [
    '//flare/rpc:rpc_options_proto',
  ],
  visibility = 'PUBLIC',
)

cc_flare_library(
  name = 'echo_service_proto_flare',
  srcs = 'echo_service.proto',
  deps = ':echo_service_proto',
  visibility = 'PUBLIC',
)

proto_library(
  name = 'relay_service_proto',
  srcs = 'relay_service.proto',
  deps = [
    '//flare/rpc:rpc_options_proto',
  ],
  visibility = 'PUBLIC',
)

cc_flare_library(
  name = 'relay_service_proto_flare',
  srcs = 'relay_service.proto',
  deps = ':relay_service_proto',
  visibility = 'PUBLIC',
)

cc_library(
  name = 'http_mock',
  hdrs = 'http_mock.h',
  srcs = 'http_mock.cc',
  deps = [
    '//flare/base/internal:lazy_init',
    '//flare/init:on_init',
    '//flare/net/http:http_client',
    '//flare/testing/detail:gmock_actions',
    '//thirdparty/googletest:gmock',
    '//thirdparty/googletest:gtest',
  ],
  link_all_symbols = True,
  visibility = 'PUBLIC',
)

cc_test(
  name = 'http_mock_test',
  srcs = 'http_mock_test.cc',
  deps = [
    ':http_mock',
    ':main',
    '//flare/fiber:fiber',
  ]
)

cc_library(
  name = 'redis_mock',
  hdrs = 'redis_mock.h',
  srcs = 'redis_mock.cc',
  deps = [
    '//flare/base:function',
    '//flare/base/internal:lazy_init',
    '//flare/init:on_init',
    '//flare/net/redis:mock_channel',
    '//flare/net/redis:reader',
    '//flare/net/redis:redis',
    '//flare/testing/detail:gmock_actions',
    '//thirdparty/googletest:gmock',
    '//thirdparty/googletest:gtest',
  ],
  link_all_symbols = True,
  visibility = 'PUBLIC',
)

cc_test(
  name = 'redis_mock_test',
  srcs = 'redis_mock_test.cc',
  deps = [
    ':main',
    ':redis_mock',
    '//flare/net/redis:redis',
    '//thirdparty/googletest:gmock',
  ]
)

cc_library(
  name = 'naked_server',
  hdrs = 'naked_server.h',
  srcs = 'naked_server.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base:function',
    '//flare/base:handle',
    '//flare/base:ref_ptr',
    '//flare/base/net:endpoint',
    '//flare/fiber:fiber',
    '//flare/io:io_basic',
    '//flare/io/native:native',
    '//flare/io/util:socket',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'naked_server_test',
  srcs = 'naked_server_test.cc',
  deps = [
    ':endpoint',
    ':main',
    ':naked_server',
    '//flare/io/native:native',
    '//flare/io/util:socket',
  ]
)

cc_library(
  name = 'cos_mock',
  hdrs = 'cos_mock.h',
  srcs = 'cos_mock.cc',
  deps = [
    '//flare/base:demangle',
    '//flare/base:function',
    '//flare/base:logging',
    '//flare/base/internal:lazy_init',
    '//flare/init:on_init',
    '//flare/net/cos:channel',
    '//flare/net/cos:cos_client',
    '//flare/testing/detail:gmock_actions',
    '//flare/testing/detail:implicitly_casting',
    '//thirdparty/googletest:gmock',
  ],
  link_all_symbols = True,
  visibility = 'PUBLIC',
)

cc_test(
  name = 'cos_mock_test',
  srcs = 'cos_mock_test.cc',
  deps = [
    ':cos_mock',
    ':main',
    '//flare/net/cos:cos_client',
    '//flare/net/cos/ops/object:get_object',
    '//thirdparty/googletest:gmock',
  ]
)
