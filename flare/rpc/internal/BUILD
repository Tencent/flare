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
  name = 'correlation_id',
  hdrs = 'correlation_id.h',
  deps = [
    '//flare/base:id_alloc',
  ],
  visibility = [
    '//flare/net/...',
    '//flare/rpc/...',
  ],
)

cc_library(
  name = 'correlation_map',
  hdrs = 'correlation_map.h',
  deps = [
    ':sharded_call_map',
    '//flare/base:id_alloc',
    '//flare/base:never_destroyed',
    '//flare/fiber:fiber',
  ],
  visibility = ['//flare/rpc/...'],
)

cc_library(
  name = 'stream_call_gate_pool',
  hdrs = 'stream_call_gate_pool.h',
  srcs = 'stream_call_gate_pool.cc',
  deps = [
    ':stream_call_gate',
    '//flare/base:exposed_var',
    '//flare/base:function_view',
    '//flare/base:hazptr',
    '//flare/base:random',
    '//flare/base:ref_ptr',
    '//flare/base/internal:early_init',
    '//flare/base/internal:hash_map',
    '//flare/base/net:endpoint',
    '//flare/base/thread:latch',
    '//flare/fiber:fiber',
    '//flare/io:io_basic',
    '//thirdparty/gflags:gflags',
  ],
  visibility = [
    '//flare:init',
    '//flare/net/...',
    '//flare/rpc/...',
  ],
)

cc_test(
  name = 'stream_call_gate_pool_test',
  srcs = 'stream_call_gate_pool_test.cc',
  deps = [
    ':stream_call_gate',
    ':stream_call_gate_pool',
    '//flare/fiber:fiber',
    '//flare/testing:main',
    '//flare/testing:endpoint',
    '//flare/rpc:rpc',
  ]
)

cc_library(
  name = 'buffered_stream_provider',
  hdrs = 'buffered_stream_provider.h',
  deps = [
    ':stream',
    '//flare/base:logging',
    '//flare/fiber:fiber',
  ],
  visibility = ['//flare/rpc/...'],
)

cc_test(
  name = 'buffered_stream_provider_test',
  srcs = 'buffered_stream_provider_test.cc',
  deps = [
    ':buffered_stream_provider',
    '//flare/base:maybe_owning',
    '//flare/testing:main',
  ]
)

cc_library(
  name = 'error_stream_provider',
  hdrs = 'error_stream_provider.h',
  deps = [':stream'],
  visibility = ['//flare/rpc/...'],
)

cc_test(
  name = 'error_stream_provider_test',
  srcs = 'error_stream_provider_test.cc',
  deps = [
    ':error_stream_provider',
    '//flare/base:maybe_owning',
    '//flare/testing:main',
  ]
)

cc_library(
  name = 'stream_io_adaptor',
  hdrs = 'stream_io_adaptor.h',
  srcs = 'stream_io_adaptor.cc',
  deps = [
    ':buffered_stream_provider',
    ':stream',
    '//flare/base:function',
    '//flare/base:function',
    '//flare/base:maybe_owning',
    '//flare/base:ref_ptr',
    '//flare/base:ref_ptr',
    '//flare/fiber:fiber',
    '//flare/fiber:work_queue',
    '//flare/rpc/protocol:message',
  ],
  visibility = ['//flare/rpc/...'],
)

cc_library(
  name = 'sharded_call_map',
  hdrs = 'sharded_call_map.h',
  deps = [
    '//flare/base:align',
    '//flare/base/internal:hash_map',
  ],
  visibility = ['//flare/rpc/...'],
)

cc_library(
  name = 'rpc_metrics',
  hdrs = 'rpc_metrics.h',
  srcs = 'rpc_metrics.cc',
  deps = [
    '//flare/base:never_destroyed',
    '//flare/base:write_mostly',
    '//flare/base/internal:annotation',
    '//flare/rpc/protocol/protobuf:rpc_meta_proto',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
  visibility = [
    '//flare/net/hbase/...',
    '//flare/rpc/...',
  ],
)

cc_test(
  name = 'rpc_metrics_test',
  srcs = 'rpc_metrics_test.cc',
  deps = [
    ':rpc_metrics',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:main',
    '//thirdparty/jsoncpp:jsoncpp',
  ]
)

cc_benchmark(
  name = 'rpc_metrics_benchmark',
  srcs = 'rpc_metrics_benchmark.cc',
  deps = [
    ':rpc_metrics',
    '//flare/testing:echo_service_proto_flare',
  ]
)

cc_library(
  name = 'fast_latch',
  hdrs = 'fast_latch.h',
  srcs = 'fast_latch.cc',
  deps = [
    '//flare/base:callback',
    '//flare/fiber:fiber',
  ],
  visibility = [
    '//flare/net/hbase/...',
    '//flare/rpc/...',
  ],
)

cc_test(
  name = 'fast_latch_test',
  srcs = 'fast_latch_test.cc',
  deps = [
    ':fast_latch',
    '//flare/base:random',
    '//flare/fiber:fiber',
    '//flare/testing:main',
  ]
)

cc_library(
  name = 'sampler',
  hdrs = 'sampler.h',
  deps = [
    '//flare/base:chrono',
    '//flare/base:likely',
    '//flare/base:logging',
    '//flare/base/thread:thread_local',
  ],
  visibility = ['//flare/rpc/...'],
)

cc_test(
  name = 'sampler_test',
  srcs = 'sampler_test.cc',
  deps = [
    ':sampler',
    '//flare/base:chrono',
  ]
)

cc_library(
  name = 'stream_call_gate',
  hdrs = 'stream_call_gate.h',
  srcs = ['stream_call_gate.cc'],
  deps = [
    ':correlation_id',
    ':correlation_map',
    ':sharded_call_map',
    ':stream',
    ':stream_io_adaptor',
    '//flare/base:align',
    '//flare/base:delayed_init',
    '//flare/base:function',
    '//flare/base:maybe_owning',
    '//flare/base:object_pool',
    '//flare/base/internal:early_init',
    '//flare/base/internal:test_prod',
    '//flare/base/net:endpoint',
    '//flare/fiber:fiber',
    '//flare/io:io_basic',
    '//flare/io/native:native',
    '//flare/io/util:socket',
    '//flare/rpc/binlog:binlog',
    '//flare/rpc/protocol:stream_protocol',
    '//thirdparty/gflags:gflags',
  ],
  visibility = [
    '//flare/net/...',
    '//flare/rpc/...',
  ],
)

# cc_test(
#   name = 'stream_call_gate_test',
#   srcs = [],
#   deps = [
#     ':stream_call_gate',
#   ]
# )

cc_library(
  name = 'stream',
  hdrs = 'stream.h',
  srcs = [],
  deps = [
    '//flare/base:expected',
    '//flare/base:function',
    '//flare/base:future',
    '//flare/base:ref_ptr',
    '//flare/fiber:future',
    '//thirdparty/protobuf:protobuf',
  ],
  visibility = ['//flare/rpc/...'],
)

cc_test(
  name = 'stream_test',
  srcs = 'stream_test.cc',
  deps = [
    ':stream',
    '//flare/fiber:fiber',
    '//flare/testing:main',
    '//thirdparty/googletest:gmock',
  ]
)

cc_library(
  name = 'session_context',
  hdrs = 'session_context.h',
  srcs = 'session_context.cc',
  deps = [
    '//flare/base:object_pool',
    '//flare/base:ref_ptr',
    '//flare/fiber:fiber',
    '//flare/rpc/binlog:log_reader',
    '//flare/rpc/binlog:log_writer',
    '//flare/rpc/tracing:tracing_ops',
  ],
  visibility = 'PUBLIC',
)
