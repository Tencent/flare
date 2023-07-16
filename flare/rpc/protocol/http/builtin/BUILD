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
  name = 'builtin',
  hdrs = [],
  deps = [
    ':exposed_vars_handler',
    ':gflags_handler',
    ':misc_handler',
    ':options_handler',
    ':prof_cpu_handler',
    ':rpc_form_handler',
    ':rpc_reflect_handler',
    ':rpc_statistics_handler',
    ':static_resource_http_handler',
  ],
  visibility = ['//flare/rpc:*'],
)

cc_library(
  name = 'exposed_vars_handler',
  hdrs = 'exposed_vars_handler.h',
  srcs = 'exposed_vars_handler.cc',
  deps = [
    '//flare/base:exposed_var',
    '//flare/base:string',
    '//flare/rpc:http',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
  link_all_symbols = True
)

cc_test(
  name = 'exposed_vars_handler_test',
  srcs = 'exposed_vars_handler_test.cc',
  deps = [
    ':exposed_vars_handler',
    '//thirdparty/jsoncpp:jsoncpp',
  ]
)

cc_library(
  name = 'gflags_handler',
  hdrs = 'gflags_handler.h',
  srcs = 'gflags_handler.cc',
  deps = [
    '//flare/base:string',
    '//flare/rpc:http',
    '//thirdparty/gflags:gflags',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
  link_all_symbols = True
)

cc_test(
  name = 'gflags_handler_test',
  srcs = 'gflags_handler_test.cc',
  deps = [
    ':gflags_handler',
    '//thirdparty/gflags:gflags',
    '//thirdparty/jsoncpp:jsoncpp',
  ]
)

cc_library(
  name = 'options_handler',
  hdrs = 'options_handler.h',
  srcs = 'options_handler.cc',
  deps = [
    '//flare/base:option',
    '//flare/base:string',
    '//flare/rpc:http',
    '//thirdparty/gflags:gflags',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
  link_all_symbols = True
)

cc_test(
  name = 'options_handler_test',
  srcs = 'options_handler_test.cc',
  deps = [
    ':options_handler',
    '//flare/base:option',
    '//thirdparty/gflags:gflags',
    '//thirdparty/jsoncpp:jsoncpp',
  ]
)

cc_library(
  name = 'misc_handler',
  hdrs = 'misc_handler.h',
  srcs = 'misc_handler.cc',
  deps = [
    '//flare/base:string',
    '//flare/rpc:http',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
  link_all_symbols = True
)

cc_test(
  name = 'misc_handler_test',
  srcs = 'misc_handler_test.cc',
  deps = [
    ':misc_handler',
    '//flare/base:down_cast',
    '//thirdparty/jsoncpp:jsoncpp',
  ]
)

cc_library(
  name = 'rpc_statistics_handler',
  hdrs = 'rpc_statistics_handler.h',
  srcs = 'rpc_statistics_handler.cc',
  deps = [
    '//flare/base:function',
    '//flare/rpc:http',
    '//flare/rpc/internal:rpc_metrics',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
  link_all_symbols = True
)

cc_test(
  name = 'rpc_statistics_handler_test',
  srcs = 'rpc_statistics_handler_test.cc',
  deps = [
    ':rpc_statistics_handler',
    '//flare/base:down_cast',
    '//flare/net/http:http_client',
    '//flare/rpc:rpc',
    '//flare/rpc/load_balancer:round_robin',
    '//flare/rpc/name_resolver:list',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/testing:main',
    '//thirdparty/jsoncpp:jsoncpp',
  ]
)

cc_library(
  name = 'prof_cpu_handler',
  hdrs = 'prof_cpu_handler.h',
  srcs = 'prof_cpu_handler.cc',
  deps = [
    '//flare/base/internal:logging',
    '//flare/base:string',
    '//flare/rpc:http',
    '//flare/rpc/builtin/detail:prof_utility',
    '//thirdparty/googletest:gtest_prod',
    '//thirdparty/gperftools:profiler',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
  link_all_symbols = True
)

cc_test(
  name = 'prof_cpu_handler_test',
  srcs = 'prof_cpu_handler_test.cc',
  deps = [
    ':prof_cpu_handler',
  ],
)

cc_library(
  name = 'static_resource_http_handler',
  hdrs = 'static_resource_http_handler.h',
  srcs = 'static_resource_http_handler.cc',
  deps = [
    ':static_resources',
    '//flare/rpc:http',
  ],
  link_all_symbols = True
)

resource_library(
  name = 'static_resources',
  srcs = [
      'resources/static/antd-4.8.2.css',
      'resources/static/codemirror/lib/codemirror.css',
      'resources/static/codemirror/lib/codemirror.js',
      'resources/static/codemirror/mode/javascript/javascript.js',
      'resources/static/forms.html',
      'resources/static/forms.js',
      'resources/static/jquery-1.11.2.min.js',
      'resources/static/jquery.json-2.2.min.js',
      'resources/template/form.html',
  ]
)

cc_library(
  name = 'rpc_form_handler',
  hdrs = 'rpc_form_handler.h',
  srcs = 'rpc_form_handler.cc',
  deps = [
    ':static_resources',
    '//flare/base:string',
    '//flare/rpc:http',
    '//thirdparty/ctemplate:ctemplate',
  ],
  link_all_symbols = True
)

cc_library(
  name = 'rpc_reflect_handler',
  hdrs = 'rpc_reflect_handler.h',
  srcs = 'rpc_reflect_handler.cc',
  deps = [
    '//flare/rpc:http',
    '//flare/rpc/protocol/protobuf:gdt_json_proto_conversion',
    '//flare/rpc/protocol/protobuf:service_method_locator',
    '//thirdparty/ctemplate:ctemplate',
    '//thirdparty/protobuf:protobuf',
  ],
  link_all_symbols = True
)

cc_test(
  name = 'rpc_reflect_handler_test',
  srcs = 'rpc_reflect_handler_test.cc',
  deps = [
    ':rpc_reflect_handler',
    '//flare/base/net:endpoint',
    '//flare/net/http:http_client',
    '//flare/rpc:rpc',
    '//flare/rpc/protocol/protobuf:service_method_locator',
    '//flare/testing:echo_service_proto_flare',
    '//flare/testing:endpoint',
    '//flare/testing:main',
    '//thirdparty/jsoncpp:jsoncpp',
    '//thirdparty/protobuf:protobuf',
  ],
)
