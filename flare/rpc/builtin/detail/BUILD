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
  name = 'uri_matcher',
  hdrs = 'uri_matcher.h',
  srcs = 'uri_matcher.cc',
  deps = [
    '//flare/base:function',
    '//flare/base:string',
    '//flare/base/encoding:base64',
  ],
  visibility = '//flare/rpc/builtin/...',
)

cc_test(
  name = 'uri_matcher_test',
  srcs = 'uri_matcher_test.cc',
  deps = [
    ':uri_matcher',
  ]
)

cc_library(
  name = 'prof_utility',
  hdrs = 'prof_utility.h',
  srcs = 'prof_utility.cc',
  deps = [
    '//flare/base:logging',
  ],
  visibility = [
    '//flare/rpc/builtin/...',
    '//flare/rpc/protocol/http/builtin/...',
  ]
)
