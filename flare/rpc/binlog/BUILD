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
  name = 'binlog',
  hdrs = [
    'dry_runner.h',
    'dumper.h',
    'init.h',
    'log_reader.h',
    'log_writer.h',
    'packet_desc.h',
    'tags.h',
  ],
  srcs = 'init.cc',
  deps = [
    ':dry_runner',
    ':dumper',
    ':log_reader',
    ':log_writer',
    ':packet_desc',
    '//flare/base:buffer',
    '//flare/base:casting',
    '//flare/base:dependency_registry',
    '//flare/base:expected',
    '//flare/base:future',
    '//flare/base:status',
    '//flare/base/experimental:lazy_eval',
    '//flare/base/experimental:uuid',
    '//flare/base/internal:early_init',
    '//flare/base/net:endpoint',
    '//flare/base/net:endpoint',
    '//flare/fiber:fiber',
    '//flare/rpc/binlog/text_only:dumper',
    '//thirdparty/gflags:gflags',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'tags',
  hdrs = 'tags.h',
  visibility = 'PUBLIC',
)

cc_library(
  name = 'packet_desc',
  hdrs = [
    'packet_desc.h',
  ],
  srcs = 'packet_desc.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base:casting',
    '//flare/base/buffer:zero_copy_stream',
    '//flare/base/experimental:lazy_eval',
    '//flare/base/internal:early_init',
    '//thirdparty/protobuf:protobuf',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'dumper',
  hdrs = 'dumper.h',
  srcs = 'dumper.cc',
  deps = [
    ':packet_desc',
    '//flare/base:buffer',
    '//flare/base:dependency_registry',
    '//flare/base:id_alloc',
    '//flare/base:maybe_owning',
    '//flare/base:never_destroyed',
    '//flare/base:string',
    '//flare/base/experimental:lazy_eval',
    '//flare/base/experimental:uuid',
    '//flare/base/internal:index_alloc',  # ???
    '//flare/base/net:endpoint',
    '//flare/rpc/binlog:tags',
    '//flare/rpc/internal:sampler',
    '//thirdparty/gflags:gflags',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'dumper_test',
  srcs = 'dumper_test.cc',
  deps = [
    ':dumper',
    ':testing',
    '//flare/base:down_cast',
    '//flare/base:string',
    '//flare/rpc/binlog:tags',
    '//flare/rpc/binlog/text_only:dumper',
    '//thirdparty/gflags:gflags',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
  exclusive = True
)

cc_library(
  name = 'log_reader',
  hdrs = 'log_reader.h',
  srcs = 'log_reader.cc',
  deps = [
    ':dry_runner',
    '//flare/base:string',
    '//flare/base/experimental:uuid',
    '//flare/fiber:fiber',
    '//flare/rpc/binlog:tags',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'log_writer',
  hdrs = 'log_writer.h',
  srcs = 'log_writer.cc',
  deps = [
    ':dumper',
    '//flare/base:chrono',
    '//flare/base/experimental:uuid',
    '//flare/base/internal:dpc',
    '//flare/rpc/binlog:tags',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'log_writer_test',
  srcs = 'log_writer_test.cc',
  deps = [
    ':log_writer',
    ':testing',
    '//flare/base:down_cast',
    '//flare/base:string',
    '//flare/init:override_flag',
    '//flare/rpc/binlog/text_only:dumper',
    '//flare/testing:main',
    '//thirdparty/gflags:gflags',
    '//thirdparty/jsoncpp:jsoncpp',
  ]
)

cc_library(
  name = 'testing',
  hdrs = 'testing.h',
  srcs = 'testing.cc',
  deps = [
    ':dumper',
    ':packet_desc',
    '//flare/base:down_cast',
    '//flare/rpc/protocol:message',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'dry_runner',
  hdrs = 'dry_runner.h',
  srcs = 'dry_runner.cc',
  deps = [
    ':packet_desc',
    '//flare/base:buffer',
    '//flare/base:dependency_registry',
    '//flare/base:expected',
    '//flare/base:future',
    '//flare/base:never_destroyed',
    '//flare/base:status',
    '//flare/base:string',
    '//flare/base/experimental:uuid',
    '//flare/base/net:endpoint',
    '//flare/fiber:fiber',
    '//thirdparty/gflags:gflags',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'dry_runner_test',
  srcs = 'dry_runner_test.cc',
  deps = [
    ':dry_runner',
    '//thirdparty/gflags:gflags',
  ]
)

cc_test(
  name = 'dry_runner_integration_test',
  srcs = 'dry_runner_integration_test.cc',
  deps = [
    ':binlog',
    '//flare/base:buffer',
    '//flare/base:deferred',
    '//flare/base:down_cast',
    '//flare/base:string',
    '//flare/base/encoding:hex',
    '//flare/base/experimental:uuid',
    '//flare/base/internal:curl',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
    '//flare/rpc/binlog/util:proto_binlog_proto',
    '//flare/rpc/binlog/util:proto_dry_runner',
    '//flare/rpc/protocol/protobuf:binlog_proto',
    '//flare/rpc/protocol/protobuf:rpc_meta_proto',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/testing:main',
    '//thirdparty/gflags:gflags',
    '//thirdparty/jsoncpp:jsoncpp',
  ]
)

cc_test(
  name = 'dumper_integration_test',
  srcs = 'dumper_integration_test.cc',
  deps = [
    ':binlog',
    '//flare/base:down_cast',
    '//flare/base:string',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
    '//flare/rpc/binlog/util:easy_dumping_log',
    '//flare/rpc/binlog/util:proto_binlog_proto',
    '//flare/rpc/binlog/util:proto_dumper',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/testing:main',
    '//flare/testing:relay_service_proto_flare',
    '//thirdparty/gflags:gflags',
  ]
)
