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

# Do not use it, use `flare/rpc:protobuf` instead.
cc_library(
  name = 'protobuf',
  hdrs = [
    'rpc_channel.h',
    'rpc_client_controller.h',
    'rpc_meta.pb.h',
    'rpc_server_controller.h',
  ],
  deps = [
    ':baidu_std_protocol',
    ':poppy_protocol',
    ':proto_over_http_protocol',
    ':qzone_protocol',
    ':rpc_channel',
    ':rpc_client_controller',
    ':rpc_meta_proto',
    ':rpc_server_controller',
    ':service_method_locator',
    ':service',
    ':std_protocol',
  ],
  visibility = [
    '//flare/rpc:rpc',
    '//flare/rpc:server',
    '//flare/rpc:for_internal_use_only_protobuf_plugin_deps',
    '//flare/rpc:protobuf',
    '//flare/testing:rpc_mock',
  ]
)

proto_library(
  name = 'rpc_meta_proto',
  srcs = 'rpc_meta.proto',
  visibility = 'PUBLIC',
)

cc_library(
  name = 'binlog',
  hdrs = 'binlog.h',
  srcs = 'binlog.cc',
  deps = [
    ':message',
    '//flare/rpc/binlog:packet_desc',
  ]
)

cc_test(
  name = 'binlog_test',
  srcs = 'binlog_test.cc',
  deps = [
    ':binlog',
    ':message',
    '//flare/testing:message_proto',
  ]
)

cc_library(
  name = 'message',
  hdrs = 'message.h',
  srcs = 'message.cc',
  deps = [
    ':rpc_meta_proto',
    '//flare/base:buffer',
    '//flare/base:enum',
    '//flare/base:maybe_owning',
    '//flare/base:string',
    '//flare/base/buffer:zero_copy_stream',
    '//flare/rpc/protocol:message',
    '//thirdparty/gflags:gflags',
    '//thirdparty/protobuf:protobuf',
    '//thirdparty/gflags:gflags',
  ],
  visibility = [
    '//flare/rpc/binlog/gdt:dumper_test',
  ]
)

cc_test(
  name = 'message_test',
  srcs = 'message_test.cc',
  deps = [
    ':message',
    '//flare/testing:main',
    '//flare/testing:message_proto',
  ]
)

cc_library(
  name = 'call_context',
  hdrs = 'call_context.h',
  srcs = 'call_context.cc',
  deps = [
    '//flare/base:logging',
    '//flare/base:maybe_owning',
    '//flare/base:object_pool',
    '//flare/rpc/protocol:controller',
    '//thirdparty/protobuf:protobuf',
  ]
)

cc_library(
  name = 'call_context_factory',
  hdrs = 'call_context_factory.h',
  srcs = 'call_context_factory.cc',
  deps = [
    ':call_context',
    ':rpc_server_controller',
    '//flare/rpc/protocol:controller',
  ]
)

cc_library(
  name = 'service_method_locator',
  hdrs = 'service_method_locator.h',
  srcs = 'service_method_locator.cc',
  deps = [
    '//flare/base:erased_ptr',
    '//flare/base:function',
    '//flare/base:logging',
    '//flare/base:never_destroyed',
    '//flare/base:type_index',
    '//flare/base/internal:annotation',
    '//flare/base/internal:hash_map',
    '//flare/init:on_init',
    '//thirdparty/protobuf:protobuf',
  ],
  visibility = [
    '//flare/rpc/protocol/http/builtin:rpc_reflect_handler',
    '//flare/rpc/protocol/http/builtin:rpc_reflect_handler_test',
  ]
)

cc_test(
  name = 'service_method_locator_test',
  srcs = 'service_method_locator_test.cc',
  deps = [
    ':service_method_locator',
    '//flare/testing:main',
    '//flare/testing:echo_service_proto_flare',
  ]
)

cc_library(
  name = 'gdt_json_proto_conversion',
  hdrs = 'gdt_json_proto_conversion.h',
  srcs = 'gdt_json_proto_conversion.cc',
  deps = [
    '//flare/base:string',
    '//thirdparty/protobuf:protobuf',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
  visibility = [
    '//flare/rpc/protocol/http/builtin:rpc_reflect_handler',
  ]
)

cc_library(
  name = 'proto_over_http_protocol',
  hdrs = 'proto_over_http_protocol.h',
  srcs = 'proto_over_http_protocol.cc',
  deps = [
    # We rely on `std_protocol` to register methods to `ServiceMethodLocator`.
    ':call_context_factory',
    ':call_context',
    ':gdt_json_proto_conversion',
    ':message',
    ':rpc_meta_proto',
    ':rpc_options',
    ':service_method_locator',
    ':std_protocol',
    '//flare/base:buffer',
    '//flare/base:demangle',
    '//flare/base:down_cast',
    '//flare/base:maybe_owning',
    '//flare/base:string',
    '//flare/base/buffer:zero_copy_stream',
    '//flare/rpc/protocol:controller',
    '//flare/rpc/protocol:message',
    '//flare/rpc/protocol:stream_protocol',
    '//flare/rpc/protocol/protobuf/detail:dirty_http',
    '//thirdparty/protobuf:protobuf',
  ],
  link_all_symbols = True
)

cc_test(
  name = 'proto_over_http_protocol_test',
  srcs = 'proto_over_http_protocol_test.cc',
  deps = [
    ':call_context',
    ':proto_over_http_protocol',
    ':service_method_locator',
    '//flare/base:down_cast',
    '//flare/base:string',
    '//flare/init:on_init',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:main',
  ]
)

cc_test(
  name = 'proto_over_http_protocol_streaming_rpc_test',
  srcs = 'proto_over_http_protocol_streaming_rpc_test.cc',
  deps = [
    ':protobuf',
    '//flare/base/thread:latch',
    '//flare/base:callback',
    '//flare/base:random',
    '//flare/fiber:fiber',
    '//flare/testing:main',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/rpc:rpc',
    '//flare/rpc/name_resolver:list',
    '//flare/rpc/load_balancer:round_robin',
  ]
)

cc_library(
  name = 'std_protocol',
  hdrs = 'std_protocol.h',
  srcs = 'std_protocol.cc',
  deps = [
    ':call_context_factory',
    ':call_context',
    ':compression',
    ':message',
    ':rpc_meta_proto',
    ':service_method_locator',
    '//flare/base:buffer',
    '//flare/base:compression',
    '//flare/base:down_cast',
    '//flare/base:endian',
    '//flare/base/buffer:zero_copy_stream',
    '//flare/rpc/protocol:controller',
    '//flare/rpc/protocol:message',
    '//flare/rpc/protocol:stream_protocol',
    '//thirdparty/protobuf:protobuf',
  ],
  link_all_symbols = True
)

cc_test(
  name = 'std_protocol_test',
  srcs = 'std_protocol_test.cc',
  deps = [
    ':call_context',
    ':message',
    ':service_method_locator',
    ':std_protocol',
    '//flare/init:on_init',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:main',
  ],
)

cc_test(
  name = 'std_protocol_streaming_rpc_test',
  srcs = 'std_protocol_streaming_rpc_test.cc',
  deps = [
    ':std_protocol',
    ':protobuf',
    '//flare/fiber:fiber',
    '//flare/testing:main',
    '//flare/testing:endpoint',
    '//flare/testing:echo_service_proto_flare',
    '//flare/rpc:rpc',
    '//flare/rpc/name_resolver:list',
    '//flare/rpc/load_balancer:round_robin',
  ],
)

cc_library(
  name = 'qzone_protocol',
  hdrs = 'qzone_protocol.h',
  srcs = [
    'qzone_protocol.cc',
  ],
  deps = [
    ':call_context_factory',
    ':call_context',
    ':message',
    ':rpc_meta_proto',
    ':rpc_options',
    ':service_method_locator',
    '//flare/base:buffer',
    '//flare/base:down_cast',
    '//flare/base:overloaded',
    '//flare/base/buffer:zero_copy_stream',
    '//flare/rpc/protocol:controller',
    '//flare/rpc/protocol:message',
    '//flare/rpc/protocol:stream_protocol',
    '//flare/rpc/protocol/protobuf/detail:qzone_header',
    '//flare/rpc/protocol/protobuf/detail:utility',
    '//thirdparty/protobuf:protobuf',
  ],
  link_all_symbols = True
)

cc_test(
  name = 'qzone_protocol_test',
  srcs = 'qzone_protocol_test.cc',
  deps = [
    ':call_context',
    ':message',
    ':qzone_protocol',
    ':service_method_locator',
    '//flare/init:on_init',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:main',
  ],
)

cc_test(
  name = 'qzone_protocol_streaming_rpc_test',
  srcs = 'qzone_protocol_streaming_rpc_test.cc',
  deps = [
    ':qzone_protocol',
    ':protobuf',
    '//flare/fiber:fiber',
    '//flare/testing:main',
    '//flare/testing:endpoint',
    '//flare/testing:echo_service_proto_flare',
    '//flare/rpc:rpc',
    '//flare/rpc/name_resolver:list',
    '//flare/rpc/load_balancer:round_robin',
  ],
)

cc_library(
  name = 'rpc_client_controller',
  hdrs = 'rpc_client_controller.h',
  srcs = 'rpc_client_controller.cc',
  deps = [
    ':call_context',
    ':rpc_controller_common',
    '//flare/base:status',
    '//flare/rpc/internal:stream_call_gate_pool',
    '//flare/rpc/protocol:controller',
    '//thirdparty/gflags:gflags',
    '//thirdparty/googletest:gtest_prod',
  ],
  visibility = ['//flare/testing:rpc_controller'],
)

cc_test(
  name = 'rpc_client_controller_test',
  srcs = 'rpc_client_controller_test.cc',
  deps = [
    ':rpc_channel',
    ':rpc_client_controller',
    ':rpc_server_controller',
    '//flare/base:callback',
    '//flare/base:string',
    '//flare/fiber:fiber',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/testing:main',
  ],
)

cc_library(
  name = 'rpc_controller_common',
  hdrs = 'rpc_controller_common.h',
  srcs = [
    'rpc_controller_common_inl.h',
    'rpc_controller_common.cc',
  ],
  deps = [
    ':message',
    '//flare/base:buffer',
    '//flare/base:down_cast',
    '//flare/base:status',
    '//flare/base/internal:time_view',
    '//flare/base/net:endpoint',
    '//flare/rpc/internal:stream_call_gate_pool',
    '//flare/rpc/internal:stream_call_gate',
    '//flare/rpc/internal:stream',
    '//flare/rpc/protocol:controller',
    '//flare/rpc/protocol/protobuf:call_context',
    '//flare/rpc/protocol/protobuf:rpc_meta_proto',
  ]
)

cc_library(
  name = 'rpc_server_controller',
  hdrs = 'rpc_server_controller.h',
  srcs = 'rpc_server_controller.cc',
  deps = [
    ':rpc_controller_common',
    '//flare/base:string',
    '//flare/rpc/internal:session_context',
    '//thirdparty/googletest:gtest_prod',
    '//thirdparty/protobuf:protobuf',
  ],
  visibility = ['//flare/testing:rpc_controller'],
)

cc_test(
  name = 'rpc_server_controller_test',
  srcs = 'rpc_server_controller_test.cc',
  deps = [
    ':rpc_server_controller',
    ':rpc_client_controller',
    ':rpc_channel',
    '//flare/fiber:fiber',
    '//flare/testing:main',
    '//flare/testing:endpoint',
    '//flare/testing:echo_service_proto_flare',
    '//flare/rpc:rpc',
  ],
)

cc_library(
  name = 'service',
  hdrs = 'service.h',
  srcs = 'service.cc',
  deps = [
    ':binlog_proto',
    ':binlog',
    ':call_context',
    ':message',
    ':rpc_options',
    ':rpc_server_controller',
    ':service_method_locator',
    '//flare/base:buffer',
    '//flare/base:callback',
    '//flare/base:deferred',
    '//flare/base:down_cast',
    '//flare/base:maybe_owning',
    '//flare/base:string',
    '//flare/base/internal:hash_map',
    '//flare/fiber:fiber',
    '//flare/rpc:rpc_options_proto',
    '//flare/rpc/binlog:binlog',
    '//flare/rpc/internal:fast_latch',
    '//flare/rpc/internal:rpc_metrics',
    '//flare/rpc/internal:session_context',
    '//flare/rpc/internal:stream',
    '//flare/rpc/protocol:stream_protocol',
    '//flare/rpc/protocol:stream_service',
    '//flare/rpc/tracing:tracing',
    '//thirdparty/gflags:gflags',
    '//thirdparty/googletest:gtest_prod',
  ],
  visibility = ['//flare/rpc:*'],
)

cc_library(
  name = 'mock_channel',
  hdrs = 'mock_channel.h',
  deps = [
    '//thirdparty/protobuf:protobuf',
  ],
  visibility = [
    '//flare/testing:rpc_mock',
  ]
)

cc_library(
  name = 'rpc_channel',
  hdrs = 'rpc_channel.h',
  srcs = [
    'rpc_channel.cc',
    'rpc_channel_for_dry_run.cc',
    'rpc_channel_for_dry_run.h',
  ],
  deps = [
    ':binlog_proto',
    ':message',
    ':mock_channel',
    ':rpc_client_controller',
    ':rpc_options',
    ':service_method_locator',
    '//flare/base:callback',
    '//flare/base:chrono',
    '//flare/base:down_cast',
    '//flare/base:endian',
    '//flare/base:function',
    '//flare/base:random',
    '//flare/base:ref_ptr',
    '//flare/base:string',
    '//flare/base/buffer:zero_copy_stream',
    '//flare/base/internal:lazy_init',
    '//flare/base/net:endpoint',
    '//flare/fiber:fiber',
    '//flare/rpc/binlog:binlog',
    '//flare/rpc/internal:correlation_id',
    '//flare/rpc/internal:error_stream_provider',
    '//flare/rpc/internal:fast_latch',
    '//flare/rpc/internal:session_context',
    '//flare/rpc/internal:stream_call_gate',
    '//flare/rpc/load_balancer:load_balancer',
    '//flare/rpc/message_dispatcher:composited',
    '//flare/rpc/name_resolver:name_resolver',
    '//flare/rpc/protocol:stream_protocol',
    '//flare/rpc/tracing:tracing',
    '//thirdparty/gflags:gflags',
    '//thirdparty/googletest:gtest_prod',
    '//thirdparty/opentracing-cpp:opentracing',
  ]
)

cc_test(
  name = 'rpc_channel_test',
  srcs = 'rpc_channel_test.cc',
  deps = [
    ':message',
    ':rpc_channel',
    '//flare/testing:main',
    '//flare/testing:endpoint',
    '//flare/testing:rpc_mock',
    '//flare/testing:echo_service_proto_flare',
    '//flare/fiber:fiber',
    '//flare/rpc:rpc',
    '//flare/rpc/name_resolver:list',
    '//flare/rpc/load_balancer:round_robin',
  ],
  testdata = [
    ('testdata/', 'conf/'),
  ]
)


cc_test(
  name = 'service_test',
  srcs = 'service_test.cc',
  deps = [
    ':call_context_factory',
    ':message',
    ':rpc_server_controller',
    ':service',
    ':std_protocol',
    '//flare/fiber:fiber',
    '//flare/init:override_flag',
    '//flare/rpc/internal:stream_io_adaptor',
    '//flare/rpc/protocol:stream_protocol',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:main',
  ]
)

for block_size in ['4K', '64K', '1M']:
  cc_test(
    name = 'integration_test_' + block_size,
    srcs = 'integration_test.cc',
    deps = [
      ':protobuf',
      '//flare/base:compression',
      '//flare/base:string',
      '//flare/base/thread:latch',
      '//flare/init:override_flag',
      '//flare/rpc:rpc',
      '//flare/rpc/load_balancer:round_robin',
      '//flare/rpc/name_resolver:list',
      '//flare/testing:echo_service_proto_flare',
      '//flare/testing:endpoint',
      '//flare/testing:main',
    ],
    defs = ['BUFFER_BLOCK_SIZE=\\"%s\\"' % block_size]
  )

cc_library(
  name = 'rpc_options',
  hdrs = 'rpc_options.h',
  srcs = 'rpc_options.cc',
  deps = [
    '//flare/rpc:rpc_options_proto',
    '//flare/base:logging',
  ],
  visibility = [
    '//flare/rpc/protocol/protobuf/plugin:*',
  ]
)

proto_library(
  name = 'baidu_std_rpc_meta_proto',
  srcs = 'baidu_std_rpc_meta.proto'
)

cc_library(
  name = 'baidu_std_protocol',
  hdrs = 'baidu_std_protocol.h',
  srcs = 'baidu_std_protocol.cc',
  deps = [
    ':baidu_std_rpc_meta_proto',
    ':call_context_factory',
    ':call_context',
    ':compression',
    ':message',
    ':rpc_meta_proto',
    ':rpc_options',
    ':service_method_locator',
    ':std_protocol',  # For the same reason as of `proto_over_http`.
    '//flare/base:buffer',
    '//flare/base:down_cast',
    '//flare/base:endian',
    '//flare/base/buffer:zero_copy_stream',
    '//flare/rpc/protocol:controller',
    '//flare/rpc/protocol:message',
    '//flare/rpc/protocol:stream_protocol',
    '//thirdparty/protobuf:protobuf',
  ],
  link_all_symbols = True
)

cc_test(
  name = 'baidu_std_protocol_test',
  srcs = 'baidu_std_protocol_test.cc',
  deps = [
    ':baidu_std_protocol',
    ':call_context',
    ':message',
    ':service_method_locator',
    '//flare/init:on_init',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:main',
  ],
)

proto_library(
  name = 'poppy_rpc_meta_proto',
  srcs = 'poppy_rpc_meta.proto',
)

cc_library(
  name = 'poppy_protocol',
  hdrs = 'poppy_protocol.h',
  srcs = 'poppy_protocol.cc',
  deps = [
    ':call_context_factory',
    ':call_context',
    ':compression',
    ':message',
    ':poppy_rpc_meta_proto',
    ':rpc_meta_proto',
    ':rpc_options',
    ':service_method_locator',
    ':std_protocol',  # For the same reason as of `proto_over_http`.
    '//flare/base:buffer',
    '//flare/base:down_cast',
    '//flare/base:endian',
    '//flare/base:string',
    '//flare/base/buffer:zero_copy_stream',
    '//flare/rpc/protocol:controller',
    '//flare/rpc/protocol:message',
    '//flare/rpc/protocol:stream_protocol',
    '//thirdparty/protobuf:protobuf',
  ],
  link_all_symbols = True
)

cc_test(
  name = 'poppy_protocol_test',
  srcs = 'poppy_protocol_test.cc',
  deps = [
    ':call_context',
    ':message',
    ':poppy_protocol',
    ':poppy_rpc_meta_proto',
    ':service_method_locator',
    '//flare/base:down_cast',
    '//flare/base:endian',
    '//flare/init:on_init',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:main',
  ],
)

cc_library(
  name = 'compression',
  hdrs = 'compression.h',
  srcs = 'compression.cc',
  deps = [
    ':message',
    ':rpc_meta_proto',
    '//flare/base:buffer',
    '//flare/base:compression',
  ],
)

cc_test(
  name = 'compression_test',
  srcs = 'compression_test.cc',
  deps = [
    ':compression',
    '//flare/base:object_pool',
  ],
)

proto_library(
  name = 'binlog_proto',
  srcs = 'binlog.proto',
  visibility = [
    '//flare/rpc/binlog:binlog_dry_runner_integration_test',
    '//flare/rpc/binlog:dry_runner_integration_test',
  ],
)
