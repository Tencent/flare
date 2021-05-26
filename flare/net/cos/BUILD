# Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
  name = 'channel',
  hdrs = 'channel.h',
  deps = [
    '//flare/base:expected',
    '//flare/base:future',
    '//flare/base:status',
    '//flare/net/cos/ops:operation',
  ],
  visibility = '//flare/testing:cos_mock',
)

cc_library(
  name = 'cos_status',
  hdrs = 'cos_status.h',
  srcs = 'cos_status.cc',
  deps = [
    '//flare/base:enum',
    '//flare/base:logging',
    '//flare/base:status',
    '//flare/base:string',
    '//flare/net/http:types',
    '//thirdparty/rapidxml:rapidxml',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'cos_status_test',
  srcs = 'cos_status_test.cc',
  deps = [
    ':cos_status',
  ]
)

cc_library(
  name = 'cos_channel',
  hdrs = 'cos_channel.h',
  srcs = 'cos_channel.cc',
  deps = [
    ':channel',
    ':cos_status',
    '//flare/base:enum',
    '//flare/net/internal:http_engine',
    '//flare/rpc/message_dispatcher:message_dispatcher',
  ]
)

cc_library(
  name = 'cos_client',
  hdrs = 'cos_client.h',
  srcs = 'cos_client.cc',
  deps = [
    ':channel',
    ':cos_channel',
    '//flare/base:expected',
    '//flare/base:future',
    '//flare/base:maybe_owning',
    '//flare/base:status',
    '//flare/base:string',
    '//flare/base/internal:time_view',
    '//flare/fiber:future',
    '//flare/net/cos/ops:operation',
    '//thirdparty/gflags:gflags',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'cos_client_test',
  srcs = 'cos_client_test.cc',
  deps = [
    ':cos_client',
    ':cos_status',
    '//flare/base:buffer',
    '//flare/base:enum',
    '//flare/fiber:fiber',
    '//flare/rpc:rpc',
    '//flare/testing:endpoint',
    '//flare/testing:main',
  ]
)

cc_library(
  name = 'signature',
  hdrs = 'signature.h',
  srcs = 'signature.cc',
  deps = [
    '//flare/base:chrono',
    '//flare/base:logging',
    '//flare/base:string',
    '//flare/base/crypto:sha',
    '//flare/base/encoding:hex',
    '//flare/base/encoding:percent',
    '//flare/net/http:types',
  ],
  visibility = '//flare/net/cos/...'
)

cc_test(
  name = 'signature_test',
  srcs = 'signature_test.cc',
  deps = [
    ':signature',
  ]
)

cc_library(
  name = 'xml_reader',
  hdrs = 'xml_reader.h',
  deps = [
    '//flare/base:logging',
    '//flare/base:string',
    '//flare/base/encoding:percent',
    '//thirdparty/rapidxml:rapidxml',
  ],
  visibility = '//flare/net/cos/ops/...'
)
