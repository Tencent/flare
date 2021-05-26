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
  name = 'packing_proto',
  srcs = 'packing.proto'
)

cc_library(
  name = 'packing',
  hdrs = 'packing.h',
  srcs = 'packing.cc',
  deps = [
    ':packing_proto',
    '//flare/base:buffer',
    '//flare/base:endian',
    '//flare/base:logging',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'packing_test',
  srcs = 'packing_test.cc',
  deps = [
    ':packing',
  ]
)

cc_library(
  name = 'polymorphic_buffer',
  hdrs = 'polymorphic_buffer.h',
  deps = [
    '//flare/base:object_pool',
    '//flare/base:ref_ptr',
    '//flare/base/internal:singly_linked_list',
  ],
  visibility = ['//flare/base:buffer'],
)

cc_library(
  name = 'builtin_buffer_block',
  hdrs = 'builtin_buffer_block.h',
  srcs = 'builtin_buffer_block.cc',
  deps = [
    ':polymorphic_buffer',
    '//flare/base:align',
    '//flare/base:object_pool',
    '//flare/init:on_init',
    '//thirdparty/gflags:gflags',
  ],
  visibility = ['//flare/base:buffer'],
)

cc_library(
  name = 'zero_copy_stream',
  hdrs = 'zero_copy_stream.h',
  srcs = 'zero_copy_stream.cc',
  deps = [
    '//flare/base:buffer',
    '//thirdparty/protobuf:protobuf',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'zero_copy_stream_test',
  srcs = 'zero_copy_stream_test.cc',
  deps = [
    ':zero_copy_stream',
    '//flare/testing:message_proto',
  ],
)

cc_library(
  name = 'compression_output_stream',
  hdrs = 'compression_output_stream.h',
  srcs = 'compression_output_stream.cc',
  deps = [
    '//flare/base/compression:compression',
    '//flare/base:buffer',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'compression_output_stream_test',
  srcs = 'compression_output_stream_test.cc',
  deps = [
    ':compression_output_stream',
  ],
)

cc_library(
  name = 'view',
  hdrs = 'view.h',
  srcs = 'view.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base:logging',
    '//flare/base/internal:early_init',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'view_test',
  srcs = 'view_test.cc',
  deps = [
    ':view',
    '//flare/base:random',
    '//flare/base:string',
  ]
)
