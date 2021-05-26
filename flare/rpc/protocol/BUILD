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
  name = 'message',
  hdrs = 'message.h',
  srcs = 'message.cc',
  deps = [
    '//flare/base:casting',
    '//flare/base:enum',
    '//flare/base/internal:lazy_init',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'controller',
  hdrs = 'controller.h',
  srcs = 'controller.cc',
  deps = [
    '//flare/base:casting',
    '//flare/base/internal:lazy_init',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'stream_protocol',
  hdrs = 'stream_protocol.h',
  srcs = 'stream_protocol.cc',
  deps = [
    ':controller',
    ':message',
    '//flare/base:buffer',
    '//flare/base:dependency_registry',
    '//flare/base:enum',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'stream_service',
  hdrs = 'stream_service.h',
  deps =[
    ':controller',
    ':message',
    '//flare/base:function_view',
    '//flare/base/experimental:uuid',
    '//flare/base/net:endpoint',
    '//flare/rpc/internal:stream',
  ],
  visibility = 'PUBLIC',
)
