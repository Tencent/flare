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
  name = 'rpc',
  hdrs = [
    'http_filter.h',
    'http_handler.h',
    'logging.h',
    'rpc_channel.h',
    'rpc_client_controller.h',
    'rpc_server_controller.h',
    'server_group.h',
    'server.h',
  ],
  deps = [
    # Basic classes.
    ':server_group',
    ':server',
    ':http',
    # Frequently used NSLBs.
    '//flare/rpc/load_balancer:round_robin',
    '//flare/rpc/name_resolver:list',
    # Frequently used protocols.
    '//flare/rpc/protocol/http:http',
    '//flare/rpc/protocol/protobuf:protobuf',
  ],
  visibility = 'PUBLIC',
)

proto_library(
  name = 'rpc_options_proto',
  srcs = 'rpc_options.proto',
  visibility = 'PUBLIC',
)

cc_library(
  name = 'protobuf',
  hdrs = [
    'rpc_channel.h',
    'rpc_client_controller.h',
    'rpc_server_controller.h',
  ],
  deps = '//flare/rpc/protocol/protobuf:protobuf',
  visibility = 'PUBLIC',
)

cc_library(
  name = 'http',
  hdrs = [
    'http_filter.h',
    'http_handler.h',
  ],
  deps = [
    '//flare/rpc/protocol/http:http_filter',
    '//flare/rpc/protocol/http:http_handler',
    '//flare/rpc/protocol/http:http',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'server',
  hdrs = [
    'server.h',
  ],
  srcs = [
    'internal/dry_run_connection_handler.cc',
    'internal/dry_run_connection_handler.h',
    'internal/normal_connection_handler.cc',
    'internal/normal_connection_handler.h',
    'internal/server_connection_handler.h',
    'server.cc',
  ],
  deps = [
    ':http',
    '//flare/base:chrono',
    '//flare/base:deferred',
    '//flare/base:down_cast',
    '//flare/base:exposed_var',
    '//flare/base:handle',
    '//flare/base:likely',
    '//flare/base:maybe_owning',
    '//flare/base:string',
    '//flare/base:type_index',
    '//flare/base/experimental:uuid',
    '//flare/base/internal:dpc',
    '//flare/base/net:endpoint',
    '//flare/fiber:fiber',
    '//flare/io:io_basic',
    '//flare/io:stream_connection',
    '//flare/io/native:native',
    '//flare/io/util:rate_limiter',
    '//flare/io/util:socket',
    '//flare/rpc/binlog:binlog',
    '//flare/rpc/binlog:log_reader',
    '//flare/rpc/internal:session_context',
    '//flare/rpc/internal:stream_io_adaptor',
    '//flare/rpc/protocol:stream_protocol',
    '//flare/rpc/protocol:stream_service',
    '//flare/rpc/protocol/http:http',
    '//flare/rpc/protocol/http/builtin:builtin',
    '//flare/rpc/protocol/protobuf:protobuf',
    '//flare/rpc/protocol/protobuf:service',
    '//flare/rpc/tracing:tracing',
    '//thirdparty/gflags:gflags',
    '//thirdparty/googletest:gtest_prod',
    '//thirdparty/jsoncpp:jsoncpp',
    '//thirdparty/opentracing-cpp:opentracing',
  ],
)

cc_library(
  name = 'server_group',
  hdrs = [
    'server_group.h',
  ],
  srcs = 'server_group.cc',
  deps = [
    ':server',
    '//flare/base:maybe_owning',
    '//flare/base/net:endpoint',
  ],
)

cc_library(
  name = 'logging',
  hdrs = 'logging.h',
  deps = [
    '//flare/fiber:logging',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'message_dispatcher_factory',
  hdrs = 'message_dispatcher_factory.h',
  srcs = 'message_dispatcher_factory.cc',
  deps = [
    '//flare/base:function',
    '//flare/base:never_destroyed',
    '//flare/base/internal:hash_map',
    '//flare/base/internal:macro',
    '//flare/rpc/load_balancer:load_balancer',
    '//flare/rpc/message_dispatcher:composited',
    '//flare/rpc/message_dispatcher:message_dispatcher',
    '//flare/rpc/name_resolver:name_resolver',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'message_dispatcher_factory_test',
  srcs = 'message_dispatcher_factory_test.cc',
  deps = [
    ':message_dispatcher_factory',
    '//flare/base:string',
    '//flare/rpc/name_resolver:name_resolver',
    '//flare/rpc/load_balancer:load_balancer',
    '//flare/rpc/message_dispatcher:message_dispatcher',
    '//flare/rpc/message_dispatcher:composited',
  ]
)

#############################################
# TARGET BELOW ARE FOR INTERNAL USE.        #
#                                           #
# YOU SHOULD ALWAYS DEPEND ON TARGET `rpc`. #
#############################################

cc_library(
  name = 'for_internal_use_only_protobuf_plugin_deps',
  hdrs = [],
  deps = [
    '//flare/base:expected',
    '//flare/base:future',
    '//flare/base:maybe_owning',
    '//flare/base:status',
    '//flare/rpc/protocol/protobuf:protobuf',
  ],
  visibility = 'PUBLIC',
)

#  Not implemented yet.
# c_library(
#  name = 'datagram_rpc_server',
#  hdrs = 'datagram_rpc_server.h',
#  srcs = ['datagram_rpc_server.cc'],
#  deps = ['//flare/io:io'],
#
#
# c_test(
#  name = 'datagram_rpc_server_test',
#  srcs = [],
#  deps = []
#

cc_test(
  name = 'server_test',
  srcs = ['server_test.cc'],
  deps = [
    ':http',
    ':rpc',
    '//flare/fiber:fiber',
    '//flare/net/http:http_client',
    '//flare/rpc/internal:stream_call_gate',
    '//flare/rpc/protocol:stream_service',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/testing:main',
    '//thirdparty/jsoncpp:jsoncpp',
  ]
)

cc_test(
  name = 'server_group_test',
  srcs = 'server_group_test.cc',
  deps = [
    ':rpc',
    ':server_group',
    '//flare/net/http:http_client',
    '//flare/rpc:server',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/testing:main',
  ]
)

cc_test(
  name = 'integration_test',
  srcs = 'integration_test.cc',
  deps = [
    ':rpc',
    '//flare/base:down_cast',
    '//flare/base:random',
    '//flare/base/internal:cpu',
    '//flare/fiber:fiber',
    '//flare/init:override_flag',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/testing:main',
    '//flare/testing:relay_service_proto_flare',
  ],
  # Internally we also redistribute this UT to dedicated test machine. Linking
  # statically make redistribution a bit easier.
  dynamic_link = False,
)

cc_test(
  name = 'logging_test',
  srcs = 'logging_test.cc',
  deps = [
    ':rpc',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/testing:main',
    '//thirdparty/googletest:gmock',
  ]
)
