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
  name = 'http_task',
  hdrs = 'http_task.h',
  srcs = 'http_task.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base:enum',
    '//flare/base:expected',
    '//flare/base:function',
    '//flare/base:object_pool',
    '//flare/base:status',
    '//flare/base/internal:lazy_init',
    '//flare/net/http:types',
    '//thirdparty/curl:curl',
  ],
  visibility = '//flare/net/...'
)

cc_test(
  name = 'http_task_test',
  srcs = 'http_task_test.cc',
  deps = [
    ':http_task',
    '//flare/base:object_pool',
    '//flare/testing:main',
  ]
)

cc_library(
  name = 'http_engine',
  hdrs = 'http_engine.h',
  srcs = 'http_engine.cc',
  deps = [
    ':http_task',
    '//flare/base:expected',
    '//flare/base:function',
    '//flare/base:never_destroyed',
    '//flare/base:object_pool',
    '//flare/base:status',
    '//flare/base/internal:lazy_init',
    '//flare/base/thread:attribute',
    '//flare/fiber:fiber',
    '//flare/fiber/detail:fiber_impl',  # !!!
    '//flare/net/http:http_response',
    '//thirdparty/curl:curl',
    '//thirdparty/gflags:gflags',
  ],
  visibility = [
    '//flare:init',
    '//flare/net/...',
  ]
)

cc_test(
  name = 'http_engine_test',
  srcs = 'http_engine_test.cc',
  deps = [
    ':http_engine',
    '//flare/base:string',
    '//flare/base/thread:latch',
    '//flare/fiber:fiber',
    '//flare/rpc:rpc',
    '//flare/testing:endpoint',
    '//flare/testing:main',
  ]
)
