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

proto_library(
  name = 'echo_service_proto',
  srcs = 'echo_service.proto',
  deps = [
    '//flare/rpc:rpc_options_proto',
  ],
  # Well, not exactly. This is mostly used for example programs.
  visibility = 'PUBLIC',
)

cc_flare_library(
  name = 'echo_service_proto_flare',
  srcs = 'echo_service.proto',
  deps = [
    ':echo_service_proto',
  ],
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
  deps = [
    ':relay_service_proto',
  ],
  visibility = 'PUBLIC',
)

cc_binary(
  name = 'server',
  srcs = [
    'server.cc',
  ],
  deps = [
    ':echo_service_proto_flare',
    '//flare:init',
    '//flare/base:exposed_var',
    '//flare/base:random',
    '//flare/fiber:fiber',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
    '//thirdparty/gflags:gflags',
    '//thirdparty/gperftools:tcmalloc_and_profiler',
  ]
)

cc_binary(
  name = 'relay_server',
  srcs = 'relay_server.cc',
  deps = [
    ':echo_service_proto_flare',
    ':relay_service_proto_flare',
    '//flare:init',
    '//flare/base:exposed_var',
    '//flare/base:monitoring',
    '//flare/fiber:fiber',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
    '//thirdparty/gflags:gflags',
    '//thirdparty/gperftools:tcmalloc_and_profiler',
  ]
)

cc_binary(
  name = 'client',
  srcs = [
    'client.cc',
  ],
  deps = [
    ':echo_service_proto_flare',
    '//flare:init',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
    '//thirdparty/gflags:gflags',
  ]
)

cc_binary(
  name = 'client2',
  srcs = [
    'client2.cc',
  ],
  deps = [
    ':echo_service_proto_flare',
    ':relay_service_proto_flare',
    '//flare:init',
    '//flare/base/thread:latch',
    '//flare/fiber:fiber',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
  ]
)

cc_binary(
  name = 'async_client',
  srcs = [
    'async_client.cc',
  ],
  deps = [
    ':echo_service_proto_flare',
    '//flare:init',
    '//flare/fiber:fiber',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
  ]
)

cc_binary(
  name = 'press',
  srcs = [
    'press.cc',
  ],
  deps = [
    ':echo_service_proto_flare',
    ':relay_service_proto',
    '//flare:init',
    '//flare/base:callback',
    '//flare/fiber:fiber',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
    '//thirdparty/gperftools:tcmalloc_and_profiler',
  ],
)
