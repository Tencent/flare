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
  name = 'operation',
  hdrs = 'operation.h',
  deps = [
    ':task',
  ],
  visibility = '//flare/net/cos/...'
)

cc_library(
  name = 'task',
  hdrs = 'task.h',
  srcs = 'task.cc',
  deps = [
    '//flare/base/crypto:md5',
    '//flare/base/encoding:base64',
    '//flare/base/net:endpoint',
    '//flare/net/cos:signature',
    '//flare/net/http:http_headers',
    '//flare/net/internal:http_task',
  ],
  visibility = '//flare/net/cos/...'
)
