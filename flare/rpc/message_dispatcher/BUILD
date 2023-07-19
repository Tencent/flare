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
  name = 'message_dispatcher',
  hdrs = 'message_dispatcher.h',
  srcs = 'message_dispatcher.cc',
  deps = [
    '//flare/base:dependency_registry',
    '//flare/base/net:endpoint',
    '//flare/rpc/load_balancer:load_balancer',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'composited',
  hdrs = 'composited.h',
  srcs = 'composited.cc',
  deps = [
    ':message_dispatcher',
    '//flare/rpc/load_balancer:load_balancer',
    '//flare/rpc/name_resolver:name_resolver',
  ],
  link_all_symbols = True,
  visibility = 'PUBLIC',
)

cc_test(
  name = 'composited_test',
  srcs = [],
  deps = []
)
