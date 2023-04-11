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
  name = 'endpoint',
  hdrs = 'endpoint.h',
  srcs = 'endpoint.cc',
  deps = [
    '//flare/base:likely',
    '//flare/base:logging',
    '//flare/base:object_pool',
    '//flare/base:ref_ptr',
    '//flare/base:string',
    '//flare/base:expected',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'endpoint_test',
  srcs = 'endpoint_test.cc',
  deps = [
    ':endpoint',
    '//flare/base:string',
  ]
)

cc_benchmark(
  name = 'endpoint_benchmark',
  srcs = 'endpoint_benchmark.cc',
  deps = [
    ':endpoint',
  ]
)

cc_library(
  name = 'uri',
  hdrs = 'uri.h',
  srcs = 'uri.cc',
  deps = [
    '//flare/base:likely',
    '//flare/base:logging',
    '//flare/base:string',
    '//flare/base/experimental:byte_set',
    '//thirdparty/gflags:gflags',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'uri_test',
  srcs = 'uri_test.cc',
  deps = [
    ':uri',
    '//flare/base:string',
  ]
)

cc_test(
  name = 'uri_extension_test',
  srcs = 'uri_extension_test.cc',
  deps = [
    ':uri',
    '//flare/base:string',
    '//flare/init:override_flag',
    '//flare/testing:main',
  ]
)
