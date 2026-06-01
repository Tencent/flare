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
  name = 'key',
  hdrs = 'key.h',
  srcs = 'key.cc',
  deps = [
    '//flare/base/internal:hash_map',
    '//flare/base:string',
  ],
  visibility = ['//flare/base:option'],
)

cc_test(
  name = 'key_test',
  srcs = 'key_test.cc',
  deps = ':key'
)

cc_test(
  name = 'option_service_test',
  srcs = 'option_service_test.cc',
  deps = [
    '//flare/base:option',
    '//thirdparty/gflags:gflags',
  ],
  visibility = ['//flare/base:option'],
)

cc_library(
  name = 'json_parser',
  hdrs = 'json_parser.h',
  srcs = 'json_parser.cc',
  deps = [
    '//thirdparty/jsoncpp:jsoncpp'
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'json_parser_test',
  srcs = 'json_parser_test.cc',
  deps = [
    ':json_parser',
  ]
)

cc_library(
  name = 'proto_parser',
  hdrs = 'proto_parser.h',
  deps = [
    '//thirdparty/protobuf:protobuf',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'proto_parser_test',
  srcs = 'proto_parser_test.cc',
  deps = [
    ':proto_parser',
    '//flare/testing:message_proto',
  ]
)
