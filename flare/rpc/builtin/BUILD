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

# If online memory profiling is desired, you need to link the HTTP handler
# CORRESPONDING TO THE MEMORY ALLOCATOR YOU'RE USING. Linking in more than one
# memory profilers will likely crash your program.
#
# The framework can't do this automatically for you, sorry.

cc_library(
  name = 'tcmalloc_profiler_http_handler',
  hdrs = [],
  srcs = [
    'tcmalloc_profiler_http_handler.cc',
    'tcmalloc_profiler_http_handler.h',
  ],
  deps = [
    '//flare/rpc:http',
    '//flare/rpc/builtin/detail:prof_utility',
    '//thirdparty/gperftools:tcmalloc_and_profiler',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
  link_all_symbols = True,
  visibility = 'PUBLIC',
)

cc_library(
  name = 'jemalloc_profiler_http_handler',
  hdrs = [],
  srcs = [
    'jemalloc_profiler_http_handler.cc',
    'jemalloc_profiler_http_handler.h',
  ],
  deps = [
    '//flare/base:deferred',
    '//flare/rpc:http',
    '//flare/rpc/builtin/detail:prof_utility',
    '//thirdparty/jemalloc:jemalloc',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
  link_all_symbols = True,
  visibility = 'PUBLIC',
)

cc_library(
  name = 'network_location_http_filter',
  hdrs = 'network_location_http_filter.h',
  srcs = 'network_location_http_filter.cc',
  deps = [
    '//flare/base:function',
    '//flare/base/internal:hash_map',
    '//flare/base/net:endpoint',
    '//flare/rpc:http',
    '//flare/rpc/builtin/detail:uri_matcher',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'network_location_http_filter_test',
  srcs = 'network_location_http_filter_test.cc',
  deps = [
    ':network_location_http_filter',
    '//flare/testing:main',
  ]
)


cc_library(
  name = 'basic_authentication_http_filter',
  hdrs = 'basic_authentication_http_filter.h',
  srcs = 'basic_authentication_http_filter.cc',
  deps = [
    '//flare/base:function',
    '//flare/base/encoding:base64',
    '//flare/base/internal:hash_map',
    '//flare/base/net:endpoint',
    '//flare/rpc:http',
    '//flare/rpc/builtin/detail:uri_matcher',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'basic_authentication_http_filter_test',
  srcs = 'basic_authentication_http_filter_test.cc',
  deps = [
    ':basic_authentication_http_filter',
    '//flare/base:string',
    '//flare/net/http:http_client',
    '//flare/rpc:http',
    '//flare/rpc:rpc',
    '//flare/testing:endpoint',
    '//flare/testing:main',
  ]
)
