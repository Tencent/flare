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

cc_binary(
  name = 'redis_client',
  srcs = 'redis_client.cc',
  deps = [
    '//flare:init',
    '//flare/base:string',
    '//flare/init:override_flag',
    '//flare/net/redis:redis',
    '//thirdparty/gflags:gflags',
  ]
)

cc_binary(
  name = 'cos_client',
  srcs = 'cos_client.cc',
  deps = [
    '//flare:init',
    '//flare/init:override_flag',
    '//flare/net/cos:cos_client',
    '//flare/net/cos/ops/bucket:get_bucket',
    '//flare/net/cos/ops/object:delete_multiple_objects',
    '//flare/net/cos/ops/object:delete_object',
    '//flare/net/cos/ops/object:get_object',
    '//flare/net/cos/ops/object:put_object',
    '//thirdparty/gflags:gflags',
  ]
)
