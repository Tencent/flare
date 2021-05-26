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

cc_binary(
  name = 'server',
  srcs = [
    'server.cc',
  ],
  deps = [
    '//flare:init',
    '//flare/base:exposed_var',
    '//flare/fiber:fiber',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
    '//flare/rpc/builtin:tcmalloc_profiler_http_handler',
    '//thirdparty/gflags:gflags',
    '//thirdparty/gperftools:tcmalloc_and_profiler',
  ]
)

cc_binary(
  name = 'je_server',
  srcs = [
    'server.cc',
  ],
  deps = [
    '//flare:init',
    '//flare/base:exposed_var',
    '//flare/fiber:fiber',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
    '//flare/rpc/builtin:jemalloc_profiler_http_handler',
    '//thirdparty/gflags:gflags',
    '//thirdparty/gperftools:profiler',
  ]
)

cc_binary(
  name = 'client',
  srcs = [
    'client.cc',
  ],
  deps = [
    '//flare:init',
    '//flare/base:enum',
    '//flare/base/thread:latch',
    '//flare/net/http:http_client',
    '//thirdparty/gflags:gflags',
    '//thirdparty/gperftools:tcmalloc_and_profiler',
  ]
)

cc_binary(
  name = 'press',
  srcs = [
    'press.cc',
  ],
  deps = [
    '//flare:init',
    '//flare/base/thread:latch',
    '//flare/fiber:fiber',
    '//flare/net/http:http_client',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
    '//flare/rpc/builtin:tcmalloc_profiler_http_handler',
    '//thirdparty/gflags:gflags',
    '//thirdparty/gperftools:tcmalloc_and_profiler',
  ]
)
