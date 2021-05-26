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
  name = 'get_object',
  hdrs = 'get_object.h',
  srcs = 'get_object.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base/encoding:percent',
    '//flare/net/cos/ops:operation',
  ],
  visibility = 'PUBLIC'
)

cc_test(
  name = 'get_object_test',
  srcs = 'get_object_test.cc',
  deps = [
    ':get_object',
  ]
)

cc_library(
  name = 'put_object',
  hdrs = 'put_object.h',
  srcs = 'put_object.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base/encoding:percent',
    '//flare/net/cos/ops:operation',
  ],
  visibility = 'PUBLIC'
)

cc_test(
  name = 'put_object_test',
  srcs = 'put_object_test.cc',
  deps = [
    ':put_object',
  ]
)

cc_library(
  name = 'delete_object',
  hdrs = 'delete_object.h',
  srcs = 'delete_object.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base/encoding:percent',
    '//flare/net/cos/ops:operation',
  ],
  visibility = 'PUBLIC'
)

cc_test(
  name = 'delete_object_test',
  srcs = 'delete_object_test.cc',
  deps = [
    ':delete_object',
  ]
)

cc_library(
  name = 'delete_multiple_objects',
  hdrs = 'delete_multiple_objects.h',
  srcs = 'delete_multiple_objects.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base/encoding:percent',
    '//flare/net/cos:xml_reader',
    '//flare/net/cos/ops:operation',
    '//thirdparty/rapidxml:rapidxml',
  ],
  visibility = 'PUBLIC'
)

cc_test(
  name = 'delete_multiple_objects_test',
  srcs = 'delete_multiple_objects_test.cc',
  deps = [
    ':delete_multiple_objects',
    '//thirdparty/rapidxml:rapidxml',
  ]
)
