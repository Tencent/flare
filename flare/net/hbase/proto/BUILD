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

proto_library(
  name = 'hbase_proto',
  srcs = 'hbase.proto',
)

proto_library(
  name = 'tracing_proto',
  srcs = 'tracing.proto',
)

proto_library(
  name = 'rpc_proto',
  srcs = 'rpc.proto',
  deps = [
    ':hbase_proto',
    ':tracing_proto',
  ],
  visibility = ['//flare/net/hbase:*']
)

cc_library(
  name = 'constants',
  hdrs = 'constants.h',
  visibility = ['//flare/net/hbase:*']
)
