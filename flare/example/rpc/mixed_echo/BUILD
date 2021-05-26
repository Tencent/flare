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
)

cc_flare_library(
  name = 'echo_service_proto_flare',
  srcs = 'echo_service.proto',
  deps = ':echo_service_proto'
)

cc_binary(
  name = 'server',
  srcs = [
    'main.cc',
  ],
  deps = [
    ':echo_service_proto_flare',
    '//flare:init',
    '//flare/init:override_flag',
    '//flare/rpc:rpc',
  ]
)
