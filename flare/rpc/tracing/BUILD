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
  name = 'tracing',
  hdrs = [
    'framework_tags.h',
    'tracing_ops_provider.h',
    'tracing_ops.h',
  ],
  deps = [
    ':framework_tags',
    ':tracing_ops_provider',
    ':tracing_ops',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'framework_tags',
  hdrs = 'framework_tags.h',
  deps = [
    '//thirdparty/opentracing-cpp:opentracing',
  ]
)

cc_library(
  name = 'string_view_interop',
  hdrs = 'string_view_interop.h',
  deps = [
    '//thirdparty/opentracing-cpp:opentracing',
  ]
)

cc_library(
  name = 'tracing_ops_provider',
  hdrs = [
    'tracing_ops_provider.h',
    'framework_tags.h',
  ],
  srcs = 'tracing_ops_provider.cc',
  deps = [
    ':framework_tags',
    '//flare/base:dependency_registry',
    '//flare/base:function',
    '//flare/base/net:endpoint',
    '//thirdparty/opentracing-cpp:opentracing',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'tracing_ops',
  hdrs = 'tracing_ops.h',
  srcs = 'tracing_ops.cc',
  deps = [
    ':tracing_ops_provider',
    '//flare/base:chrono',
    '//flare/base:exposed_var',
    '//flare/base:maybe_owning',
    '//flare/base:never_destroyed',
    '//flare/base:overloaded',
    '//flare/base:random',
    '//flare/base:string',
    '//flare/base/internal:cpu',
    '//flare/base/internal:dpc',
    '//flare/base/thread:thread_cached',
    '//flare/rpc/internal:sampler',
    '//thirdparty/gflags:gflags',
    '//thirdparty/googletest:gtest_prod',
    '//thirdparty/opentracing-cpp:opentracing',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'tracing_ops_test',
  srcs = 'tracing_ops_test.cc',
  deps = [
    ':framework_tags',
    ':tracing_ops',
    '//flare/rpc/tracing:string_view_interop',
    '//flare/testing:main',
    '//thirdparty/googletest:gmock',
  ]
)

cc_benchmark(
  name = 'tracing_ops_benchmark',
  srcs = 'tracing_ops_benchmark.cc',
  deps = [
    ':framework_tags',
    ':tracing_ops',
  ]
)

cc_test(
  name = 'integration_test',
  srcs = 'integration_test.cc',
  deps = [
    ':tracing',
    '//flare/base:overloaded',
    '//flare/base/internal:cpu',
    '//flare/fiber:fiber',
    '//flare/rpc:rpc',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/testing:main',
    '//flare/testing:relay_service_proto_flare',
    '//thirdparty/gflags:gflags',
    '//thirdparty/googletest:gmock',
    '//thirdparty/opentracing-cpp:opentracing',
  ]
)
