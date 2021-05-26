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
  name = 'dirty_hook',
  hdrs = 'dirty_hook.h',
  srcs = 'dirty_hook.cc',
  deps = [
    '//flare/base:logging',
  ],
  visibility = '//flare/testing:hooking_mock',
)

cc_test(
  name = 'dirty_hook_test',
  srcs = 'dirty_hook_test.cc',
  deps = [
    ':dirty_hook',
  ]
)

cc_library(
  name = 'gmock_actions',
  hdrs = 'gmock_actions.h',
  deps = [
    '//flare/base/internal:meta',
    '//thirdparty/googletest:gmock',
  ],
  visibility = '//flare/testing/...',
)

cc_library(
  name = 'implicitly_casting',
  hdrs = 'implicitly_casting.h',
  deps = [
    '//flare/base:down_cast',
  ],
  visibility = '//flare/testing/...',
)
