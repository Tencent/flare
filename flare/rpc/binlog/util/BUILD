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
  name = 'proto_binlog_proto',
  srcs = 'proto_binlog.proto',
  visibility = 'PUBLIC',
)

cc_library(
  name = 'easy_dumping_log',
  hdrs = 'easy_dumping_log.h',
  deps = [
    '//flare/rpc/binlog:dumper',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'proto_dumper',
  hdrs = 'proto_dumper.h',
  deps = [
    ':proto_binlog_proto',
    '//flare/rpc/binlog:dumper',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'proto_dry_runner',
  hdrs = 'proto_dry_runner.h',
  deps = [
    ':proto_binlog_proto',
    '//flare/rpc/binlog:dry_runner',
  ],
  visibility = 'PUBLIC',
)
