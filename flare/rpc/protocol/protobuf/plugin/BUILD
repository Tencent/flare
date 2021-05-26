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
  name = 'code_writer',
  hdrs = 'code_writer.h',
)

cc_library(
  name = 'names',
  hdrs = 'names.h',
  srcs = 'names.cc',
  deps = [
    ':code_writer',
    '//flare/base:string',
    '//thirdparty/protobuf:protobuf',
  ]
)

cc_library(
  name = 'async_decl_generator',
  hdrs = 'async_decl_generator.h',
  srcs = 'async_decl_generator.cc',
  deps = [
    ':code_writer',
    ':names',
    '//flare/base:string',
    '//flare/rpc/protocol/protobuf:rpc_options',
    '//thirdparty/protobuf:protoc',
  ]
)

cc_library(
  name = 'sync_decl_generator',
  hdrs = 'sync_decl_generator.h',
  srcs = 'sync_decl_generator.cc',
  deps = [
    ':code_writer',
    ':names',
    '//flare/base:string',
    '//flare/rpc/protocol/protobuf:rpc_options',
    '//thirdparty/protobuf:protoc',
  ]
)

cc_library(
  name = 'basic_decl_generator',
  hdrs = 'basic_decl_generator.h',
  srcs = 'basic_decl_generator.cc',
  deps = [
    ':code_writer',
    ':names',
    '//flare/base:string',
    '//flare/rpc/protocol/protobuf:rpc_options',
    '//thirdparty/protobuf:protoc',
  ]
)

cc_library(
  name = 'gdt_future_decl_generator',
  hdrs = 'gdt_future_decl_generator.h',
  srcs = 'gdt_future_decl_generator.cc',
  deps = [
    ':code_writer',
    ':names',
    '//flare/base:string',
    '//flare/rpc/protocol/protobuf:rpc_options',
    '//thirdparty/protobuf:protoc',
  ]
)

cc_binary(
  name = 'v1_plugin',
  srcs = [
    'v1_plugin.cc',
  ],
  deps = [
    ':async_decl_generator',
    ':basic_decl_generator',
    ':gdt_future_decl_generator',
    ':names',
    ':sync_decl_generator',
    '//flare/base:logging',
    '//flare/base:string',
    '//flare/rpc/protocol/protobuf:rpc_options',
    '//thirdparty/fmt:fmt',
    '//thirdparty/protobuf:protoc',
  ]
)

cc_binary(
  name = 'v2_plugin',
  srcs = [
    'v2_plugin.cc',
  ],
  deps = [
    ':async_decl_generator',
    ':basic_decl_generator',
    ':names',
    ':sync_decl_generator',
    '//flare/base:logging',
    '//flare/base:string',
    '//flare/base/encoding:hex',
    '//flare/rpc/protocol/protobuf:rpc_options',
    '//thirdparty/fmt:fmt',
    '//thirdparty/glog:glog',
    '//thirdparty/protobuf:protoc',
  ],
  visibility = 'PUBLIC',
)
