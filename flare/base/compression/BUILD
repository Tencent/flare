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
  name = 'compression',
  hdrs = 'compression.h',
  srcs = 'compression.cc',
  deps = [
    '//flare/base:buffer',
    '//flare/base:dependency_registry',
    '//flare/base:function',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'util',
  hdrs = 'util.h',
  srcs = 'util.cc',
  deps = [
    ':compression',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'util_test',
  srcs = 'util_test.cc',
  deps = [
    ':util',
    '//flare/base/buffer:compression_output_stream',
  ],
)

cc_library(
  name = 'gzip',
  hdrs = 'gzip.h',
  srcs = 'gzip.cc',
  deps = [
    ':compression',
    ':util',
    '//thirdparty/zlib:z',
  ],
  link_all_symbols = True,
  visibility = ['//flare/base:compression'],
)

cc_library(
  name = 'lz4',
  hdrs = 'lz4.h',
  srcs = 'lz4.cc',
  deps = [
    ':compression',
    ':util',
    '//thirdparty/lz4:lz4',
  ],
  link_all_symbols = True,
  visibility = ['//flare/base:compression'],
)

cc_test(
  name = 'compression_test',
  srcs = 'compression_test.cc',
  deps = [
    '//flare/base:compression',
    '//flare/base/buffer:compression_output_stream',
    '//flare:init',
    '//flare/testing:main',
  ],
)

cc_library(
  name = 'snappy',
  hdrs = 'snappy.h',
  srcs = 'snappy.cc',
  deps = [
    ':compression',
    ':util',
    '//thirdparty/snappy:snappy',
  ],
  link_all_symbols = True,
  visibility = ['//flare/base:compression'],
)

cc_library(
  name = 'zstd',
  hdrs = 'zstd.h',
  srcs = 'zstd.cc',
  deps = [
    ':compression',
    '//thirdparty/zstd:zstd',
  ],
  link_all_symbols = True,
  visibility = ['//flare/base:compression'],
)
