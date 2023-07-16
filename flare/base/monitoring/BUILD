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
  name = 'common',
  hdrs = 'common.h',
  srcs = 'common.cc',
  deps = [
    '//flare/base:logging',
    '//flare/base:string',
    '//flare/base/net:endpoint',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
)

cc_library(
  name = 'monitoring_system',
  hdrs = 'monitoring_system.h',
  srcs = 'monitoring_system.cc',
  deps = [
    ':event',
    '//flare/base:dependency_registry',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'dispatcher',
  hdrs = 'dispatcher.h',
  srcs = 'dispatcher.cc',
  deps = [
    ':monitoring_system',
    '//flare/base:chrono',
    '//flare/base:logging',
    '//flare/base:never_destroyed',
    '//flare/base:string',
    '//flare/base/internal:hash_map',
    '//flare/base/internal:time_keeper',
    '//thirdparty/gflags:gflags',
  ],
  visibility = ['//flare/base:monitoring'],
)

cc_library(
  name = 'event',
  srcs = 'event.cc',
  hdrs = 'event.h',
  visibility = [
    '//flare/base:monitoring'
  ],
  deps = [
    ':fwd',
    '//flare/base:likely',
    '//flare/base:logging',
    '//flare/base/experimental:flyweight',
    '//flare/base/internal:hash_map',
  ],
)

cc_library(
  name = 'fwd',
  hdrs = 'fwd.h',
  visibility = [
    '//flare/base:monitoring'
  ],
)

cc_library(
  name = 'init',
  hdrs = 'init.h',
  srcs = 'init.cc',
  deps = [
    ':dispatcher',
    ':monitoring_system',
    '//flare/base:dependency_registry',
    '//flare/base:function',
    '//flare/base:logging',
    '//flare/base:never_destroyed',
    '//flare/base:string',
    '//flare/base/internal:hash_map',
    '//thirdparty/gflags:gflags',
    '//thirdparty/yaml-cpp:yaml_cpp',
  ],
  visibility = [
    '//flare:init',
    '//flare/base:monitoring',
    '//flare/base/internal:builtin_monitoring',
  ],
)

cc_test(
  name = 'integration_test',
  srcs = 'integration_test.cc',
  deps = [
    '//flare/base:down_cast',
    '//flare/base:monitoring',
    '//flare/base/monitoring:monitoring_system',
    '//flare/init:override_flag',
    '//flare/testing:main',
    '//thirdparty/gflags:gflags',
    '//thirdparty/googletest:gmock',
  ],
  testdata = [
    ('testdata', 'conf')
  ]
)
