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
  name = 'name_resolver',
  hdrs = 'name_resolver.h',
  srcs = 'name_resolver.cc',
  deps = [
    '//flare/base:dependency_registry',
    '//flare/base/net:endpoint',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'list',
  hdrs = 'list.h',
  srcs = 'list.cc',
  deps = [
    ':name_resolver',
    ':name_resolver_impl',
    '//flare/base:logging',
    '//flare/base:string',
    '//flare/rpc/name_resolver/util:domain_name_resolver',
  ],
  link_all_symbols = True,
  visibility = 'PUBLIC',
)

cc_test(
  name = 'list_test',
  srcs = 'list_test.cc',
  deps = [
    ':list',
  ]
)

cc_library(
  name = 'name_resolver_impl',
  hdrs = [
    'name_resolver_impl.h',
    'name_resolver_updater.h',
  ],
  srcs = ['name_resolver_impl.cc', 'name_resolver_updater.cc'],
  deps = [
    '//flare/base:function',
    '//flare/base:logging',
    '//flare/base/net:endpoint',
    '//flare/base/thread:attribute',
    '//flare/rpc/name_resolver:name_resolver',
    '//thirdparty/gflags:gflags',
  ]
)

cc_test(
  name = 'name_resolver_impl_test',
  srcs = 'name_resolver_impl_test.cc',
  deps = [
    ':name_resolver_impl',
  ]
)
