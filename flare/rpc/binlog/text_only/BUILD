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

proto_library(
  name = 'binlog_proto',
  srcs = 'binlog.proto',
  deps = [
    '//flare/rpc/binlog/util:proto_binlog_proto',
  ]
)

cc_library(
  name = 'dumper',
  hdrs = 'dumper.h',
  srcs = 'dumper.cc',
  deps = [
    ':binlog_proto',
    '//flare/base:string',
    '//flare/base:logging',
    '//flare/base:buffer',
    '//flare/base:casting',
    '//flare/base:chrono',
    '//flare/net/http:http_request',
    '//flare/net/http:http_response',
    '//flare/net/http:packet_desc',
    '//flare/rpc/binlog:dumper',
    '//flare/rpc/binlog:testing',
    '//flare/rpc/binlog/util:easy_dumping_log',
    '//flare/rpc/binlog/util:proto_dumper',
    '//thirdparty/gflags:gflags',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
  link_all_symbols = True,
  visibility = [
    '//flare/rpc/binlog:binlog',
    '//flare/rpc/binlog/...',  # Used by UTs.
  ],
)

cc_test(
  name = 'dumper_test',
  srcs = 'dumper_test.cc',
  deps = [
    ':dumper',
    '//flare/base:down_cast',
    '//flare/base:encoding',
    '//flare/base:string',
    '//flare/rpc/binlog:tags',
    '//flare/rpc/binlog:testing',
    '//thirdparty/jsoncpp:jsoncpp',
  ]
)
