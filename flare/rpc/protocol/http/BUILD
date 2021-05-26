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
  name = 'http_server_context',
  hdrs = 'http_server_context.h',
  deps = [
    '//flare/base/net:endpoint',
  ]
)

cc_library(
  name = 'http_filter',
  hdrs = 'http_filter.h',
  deps = [
    ':http_server_context',
    '//flare/net/http:http_request',
    '//flare/net/http:http_response',
  ],
  visibility = '//flare/rpc:http',
)

cc_library(
  name = 'http_handler',
  hdrs = 'http_handler.h',
  srcs = 'http_handler.cc',
  deps = [
    ':http_server_context',
    '//flare/base:function',
    '//flare/base/internal:lazy_init',
    '//flare/base/net:endpoint',
    '//flare/init:on_init',
    '//flare/net/http:http_request',
    '//flare/net/http:http_response',
  ],
  visibility = '//flare/rpc:http',
)

cc_library(
  name = 'http',
  hdrs = [
    'http11_protocol.h',
    'service.h',
  ],
  srcs = [
    'http11_protocol.cc',
    'service.cc',
  ],
  deps = [
    ':binlog_proto',
    ':buffer_io',
    ':http_filter',
    ':http_handler',
    ':http_server_context',
    ':message',
    '//flare/base:buffer',
    '//flare/base:compression',
    '//flare/base:maybe_owning',
    '//flare/base:string',
    '//flare/base/buffer:compression_output_stream',
    '//flare/base/compression:util',
    '//flare/base/internal:lazy_init',
    '//flare/base/net:endpoint',
    '//flare/init:on_init',
    '//flare/net/http:packet_desc',
    '//flare/rpc/internal:session_context',
    '//flare/rpc/internal:stream',
    '//flare/rpc/protocol:controller',
    '//flare/rpc/protocol:message',
    '//flare/rpc/protocol:stream_protocol',
    '//flare/rpc/protocol:stream_service',
  ],
  visibility = [
    '//flare/rpc:http',
    '//flare/rpc:server',
    '//flare/rpc:rpc',
    '//flare/rpc/protocol/protobuf:proto_over_http_protocol',
    '//flare/net/http:http_client',
  ],
  link_all_symbols = True,
)

cc_library(
  name = 'message',
  hdrs = 'message.h',
  srcs = 'message.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/net/http:http_request',
    '//flare/net/http:http_response',
    '//flare/rpc/protocol:message',
  ],
  visibility = [
    '//flare/net/http:http_client_test',
    '//flare/rpc/protocol/protobuf:proto_over_http_protocol_test',
  ]
)

cc_test(
  name = 'http_test',
  srcs = 'http_test.cc',
  deps = [
    ':http',
    '//flare/base:string',
    '//flare/net/http:http_client',
    '//flare/rpc:rpc',
    '//flare/testing:endpoint',
    '//flare/testing:main',
    '//thirdparty/jsoncpp:jsoncpp',
  ]
)

cc_library(
  name = 'buffer_io',
  hdrs = 'buffer_io.h',
  srcs = 'buffer_io.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base:string',
    '//flare/net/http:http_headers',
    '//flare/net/http:http_request',
    '//flare/net/http:http_response',
    '//thirdparty/gflags:gflags',
  ],
  visibility = [
    '//flare/net/http:http_request_test',
    '//flare/net/http:http_response_test',
    '//flare/rpc/binlog/gdt:dry_runner_test',
  ]
)

cc_test(
  name = 'buffer_io_test',
  srcs = 'buffer_io_test.cc',
  deps = [
    ':buffer_io',
    '//flare/net/http:http_headers',
    '//flare/net/http:http_request',
    '//flare/net/http:http_response',
    '//thirdparty/googletest:gmock',
  ]
)

cc_benchmark(
  name = 'buffer_io_benchmark',
  srcs = 'buffer_io_benchmark.cc',
  deps = [
    ':buffer_io',
    '//flare/net/http:http_headers',
    '//flare/net/http:http_request',
    '//thirdparty/gflags:gflags',
  ]
)

cc_test(
  name = 'http11_protocol_test',
  srcs = 'http11_protocol_test.cc',
  deps = [
    ':http',
    ':message',
    '//flare/testing:main',
  ]
)

cc_test(
  name = 'http_handler_test',
  srcs = 'http_handler_test.cc',
  deps = [
    ':http_handler',
    ':http',
    '//flare/base:down_cast',
    '//flare/testing:main',
    '//thirdparty/googletest:gmock',
  ]
)

proto_library(
  name = 'binlog_proto',
  srcs = 'binlog.proto',
  visibility = [
    '//flare/net/http:*',
  ]
)

cc_test(
  name = 'binlog_dumper_integration_test',
  srcs = 'binlog_dumper_integration_test.cc',
  deps = [
    '//flare/base:down_cast',
    '//flare/base:string',
    '//flare/init:override_flag',
    '//flare/net/http:http_client',
    '//flare/net/http:packet_desc',
    '//flare/rpc:rpc',
    '//flare/rpc/binlog/util:easy_dumping_log',
    '//flare/rpc/binlog/util:proto_binlog_proto',
    '//flare/rpc/binlog/util:proto_dumper',
    '//flare/testing:endpoint',
    '//flare/testing:main',
  ]
)

cc_test(
  name = 'binlog_dry_runner_integration_test',
  srcs = 'binlog_dry_runner_integration_test.cc',
  deps = [
    ':binlog_proto',
    '//flare/base:buffer',
    '//flare/base:deferred',
    '//flare/base:down_cast',
    '//flare/base:string',
    '//flare/base/crypto:blake3',
    '//flare/base/encoding:hex',
    '//flare/base/experimental:uuid',
    '//flare/base/internal:curl',
    '//flare/init:override_flag',
    '//flare/net/http:http_client',
    '//flare/net/http:packet_desc',
    '//flare/rpc:rpc',
    '//flare/rpc/binlog:tags',
    '//flare/rpc/binlog/util:proto_binlog_proto',
    '//flare/rpc/binlog/util:proto_dry_runner',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/testing:main',
    '//thirdparty/gflags:gflags',
    '//thirdparty/jsoncpp:jsoncpp',
  ]
)
