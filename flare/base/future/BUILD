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
  name = 'future',
  hdrs = [
    'basics.h',
    'boxed.h',
    'core.h',
    'executor.h',
    'future.h',
    'future-impl.h',
    'impls.h',
    'promise.h',
    'promise-impl.h',
    'types.h',
    'utils.h',
  ],
  deps = [
    '//flare/base:function',
    '//flare/base/internal:logging',
  ],
  # Deprecated in favor of `flare/base:future`, but there are some targets in
  # the wild is using this library..
  visibility = 'PUBLIC',
)

cc_test(
  name = 'future_test',
  srcs = ['future_test.cc'],
  deps = [
    ':future',
    '//flare/base:future',
    '//flare/base:callback',
  ]
)
