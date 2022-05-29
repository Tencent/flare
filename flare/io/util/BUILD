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
  name = 'socket',
  hdrs = 'socket.h',
  srcs = 'socket.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base:handle',
    '//flare/base/net:endpoint',
    '//flare/fiber:alternatives',
    '//thirdparty/fmt:fmt',
  ],
  visibility = ['//flare/...'],
)

cc_test(
  name = 'socket_test',
  srcs = ['socket_test.cc'],
  deps = [
    ':socket',
    '//flare/testing:main',
    '//flare/testing:endpoint',
    '//flare/base:string',
  ]
)

cc_library(
  name = 'rate_limiter',
  hdrs = 'rate_limiter.h',
  srcs = 'rate_limiter.cc',
  deps = [
    '//flare/base:maybe_owning',
    '//flare/base:chrono',
    '//flare/base:logging',
    '//flare/base:string',
    '//thirdparty/gflags:gflags',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'rate_limiter_test',
  srcs = 'rate_limiter_test.cc',
  deps = [
    ':rate_limiter',
    '//flare/base:chrono',
    '//flare/base:random',
  ]
)

cc_test(
  name = 'rate_limiter_integration_test',
  srcs = 'rate_limiter_integration_test.cc',
  deps = [
    '//flare/fiber:fiber',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
    '//flare/rpc/load_balancer:round_robin',
    '//flare/rpc/name_resolver:list',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/testing:main',
  ]
)

cc_library(
  name = 'stream_io',
  hdrs = 'stream_io.h',
  deps = [
    '//flare/io/detail:eintr_safe',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'ssl_stream_io',
  hdrs = 'ssl_stream_io.h',
  srcs = 'ssl_stream_io.cc',
  deps = [
    ':http_proxy_stream_io',
    ':stream_io',
    '//flare/base:logging',
    '//flare/fiber:alternatives',
    '//flare/io/detail:eintr_safe',
    '//thirdparty/openssl:ssl',
  ],
)

cc_library(
  name = 'openssl',
  hdrs = 'openssl.h',
  srcs = 'openssl.cc',
  deps = [
    '//thirdparty/openssl:crypto',
    '//thirdparty/openssl:ssl',
  ],
  visibility = [
    '//flare/init:openssl_initializer',
  ]
)

cc_library(
  name = 'http_proxy_stream_io',
  hdrs = 'http_proxy_stream_io.h',
  srcs = 'http_proxy_stream_io.cc',
  deps = [
    ':stream_io',
    '//flare/base:down_cast',
    '//flare/base:string',
    '//flare/base:logging',
    '//flare/fiber:alternatives',
  ]
)
