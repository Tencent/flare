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
  name = 'http',
  hdrs = [
    'http_client.h',
    'http_headers.h',
    'http_request.h',
    'http_response.h',
    'query_string.h',
    'types.h'
  ],
  deps = [
    ':dry_run_channel',
    ':http_client',
    ':http_headers',
    ':http_request',
    ':http_response',
    ':query_string',
    ':types',
    '//flare/base:string',  # Due to BUG in blade.
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'types',
  hdrs = 'types.h',
  srcs = 'types.cc',
  deps = [
    '//flare/base:enum',
    '//flare/base:likely',
    '//flare/base:string',
    '//flare/base/internal:early_init',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'http_client',
  hdrs = 'http_client.h',
  srcs = 'http_client.cc',
  deps = [
    ':http_request',
    ':http_response',
    ':packet_desc',
    '//flare/base:expected',
    '//flare/base:function',
    '//flare/base:future',
    '//flare/base:object_pool',
    '//flare/base:string',
    '//flare/base/internal:lazy_init',
    '//flare/fiber:fiber',
    '//flare/net/internal:http_engine',
    '//flare/rpc/internal:session_context',
    '//flare/rpc/message_dispatcher:message_dispatcher',
    '//flare/rpc/protocol/http:binlog_proto',
    '//thirdparty/curl:curl',
    '//thirdparty/gflags:gflags',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'http_client_test',
  srcs = 'http_client_test.cc',
  deps = [
    ':dry_run_channel',
    ':http_client',
    '//flare/base:compression',
    '//flare/base:string',
    '//flare/base/thread:latch',
    '//flare/fiber:fiber',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
    '//flare/rpc/protocol/http:message',
    '//flare/testing:endpoint',
    '//flare/testing:main',
    '//thirdparty/curl:curl',
  ]
)

cc_library(
  name = 'dry_run_channel',
  hdrs = 'dry_run_channel.h',
  srcs = 'dry_run_channel.cc',
  deps = [
    ':http_client',
    ':packet_desc',
    '//flare/base:logging',
    '//flare/base/internal:lazy_init',
    '//flare/init:on_init',
    '//flare/rpc/binlog:tags',
    '//flare/rpc/internal:session_context',
    '//flare/rpc/protocol/http:binlog_proto',
  ],
  link_all_symbols = True,
)

cc_library(
  name = 'http_headers',
  hdrs = 'http_headers.h',
  srcs = 'http_headers.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base:logging',
    '//flare/base:string',
    '//flare/base/internal:case_insensitive_hash_map',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'http_headers_test',
  srcs = 'http_headers_test.cc',
  deps = [
    ':http_headers',
    '//thirdparty/googletest:gmock',
  ]
)

cc_library(
  name = 'http_message',
  hdrs = 'http_message.h',
  srcs = 'http_message.cc',
  deps = [
    ':http_headers',
    ':types',
    '//flare/base:buffer',
    '//flare/base:enum',
    '//flare/base:string',
    '//flare/base/internal:early_init',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'http_message_test',
  srcs = 'http_message_test.cc',
  deps = ':http_message',
)

cc_library(
  name = 'http_request',
  hdrs = 'http_request.h',
  srcs = 'http_request.cc',
  deps = [
    ':http_message',
    ':types',
    '//flare/base:enum',
    '//flare/base:string',
    '//flare/base/internal:early_init',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'http_request_test',
  srcs = 'http_request_test.cc',
  deps = [
    ':http_request',
    '//flare/rpc/protocol/http:buffer_io',
  ]
)

cc_library(
  name = 'http_response',
  hdrs = 'http_response.h',
  srcs = 'http_response.cc',
  deps = [
    ':http_message',
    ':types',
    '//flare/base:buffer',
    '//flare/base:enum',
    '//flare/base/internal:early_init',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'http_response_test',
  srcs = 'http_response_test.cc',
  deps = [
    ':http_response',
    '//flare/rpc/protocol/http:buffer_io',
  ]
)

cc_library(
  name = 'query_string',
  hdrs = 'query_string.h',
  srcs = 'query_string.cc',
  deps = [
    ':http_request',
    '//flare/base:string',
    '//flare/base/encoding:percent',
    '//flare/base/net:uri',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'query_string_test',
  srcs = 'query_string_test.cc',
  deps = [
    ':http_request',
    ':query_string',
    '//thirdparty/googletest:gmock',
  ]
)

cc_library(
  name = 'packet_desc',
  hdrs = 'packet_desc.h',
  srcs = 'packet_desc.cc',
  deps = [
    ':http_request',
    ':http_response',
    '//flare/rpc/binlog:packet_desc',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'binlog_dumper_integration_test',
  srcs = 'binlog_dumper_integration_test.cc',
  deps = [
    ':http_client',
    '//flare/base:string',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
    '//flare/rpc/binlog:binlog',
    '//flare/rpc/binlog:testing',
    '//flare/rpc/binlog/util:easy_dumping_log',
    '//flare/rpc/internal:session_context',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/testing:main',
    '//thirdparty/gflags:gflags',
  ]
)

cc_test(
  name = 'binlog_dry_runner_integration_test',
  srcs = 'binlog_dry_runner_integration_test.cc',
  deps = [
    ':dry_run_channel',
    ':http',
    '//flare/base:buffer',
    '//flare/base:deferred',
    '//flare/base:down_cast',
    '//flare/base:string',
    '//flare/base/encoding:hex',
    '//flare/base/experimental:uuid',
    '//flare/base/internal:curl',
    '//flare/init:override_flag',
    '//flare/net/http:http_client',
    '//flare/net/http:packet_desc',
    '//flare/rpc:rpc',
    '//flare/rpc/binlog:binlog',
    '//flare/rpc/binlog/util:proto_binlog_proto',
    '//flare/rpc/binlog/util:proto_dry_runner',
    '//flare/rpc/protocol/http:binlog_proto',
    '//flare/rpc/protocol/protobuf:rpc_meta_proto',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/testing:main',
    '//thirdparty/gflags:gflags',
    '//thirdparty/jsoncpp:jsoncpp',
  ]
)

cc_library(
  name = 'utility',
  hdrs = 'utility.h',
  srcs = 'utility.cc',
  deps = [
    ':http_request',
    '//flare/base/net:endpoint',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'utility_test',
  srcs = 'utility_test.cc',
  deps = [
    ':http_request',
    ':utility',
    '//flare/base:string',
  ]
)
