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
  name = 'align',
  hdrs = 'align.h',
  visibility = 'PUBLIC',
)

cc_library(
  name = 'logging',
  hdrs = 'logging.h',
  srcs = 'logging.cc',
  deps = [
    ':chrono',
    '//flare/base/internal:logging',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'logging_test',
  srcs = 'logging_test.cc',
  deps = ':logging'
)

cc_benchmark(
  name = 'logging_benchmark',
  srcs = 'logging_benchmark.cc',
  deps = ':logging'
)

cc_library(
  name = 'hazptr',
  hdrs = 'hazptr.h',
  srcs = [
    'hazptr/entry_cache.cc',
    'hazptr/entry_cache.h',
    'hazptr/entry.h',
    'hazptr/hazptr_domain.cc',
    'hazptr/hazptr_domain.h',
    'hazptr/hazptr_object.cc',
    'hazptr/hazptr_object.h',
    'hazptr/hazptr.h',
  ],
  deps = [
    ':never_destroyed',
    '//flare/base/internal:annotation',
    '//flare/base/internal:logging',
    '//flare/base/internal:memory_barrier',
    '//flare/base/internal:time_keeper',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'hazptr_test',
  srcs = 'hazptr_test.cc',
  deps = [
    ':chrono',
    ':hazptr',
    ':random',
    ':string',
    '//flare/testing:main',
  ]
)

cc_benchmark(
  name = 'hazptr_benchmark',
  srcs = 'hazptr_benchmark.cc',
  deps = ':hazptr',
)

cc_library(
  name = 'tsc',
  hdrs = 'tsc.h',
  srcs = 'tsc.cc',
  deps = [
    ':chrono',
    '//flare/base/internal:annotation',
    '//flare/base/internal:logging',
    '//thirdparty/glog:glog',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'tsc_test',
  srcs = 'tsc_test.cc',
  deps = [
    ':tsc',
    ':logging',
  ],
)

cc_benchmark(
  name = 'tsc_benchmark',
  srcs = 'tsc_benchmark.cc',
  deps = ':tsc',
)

cc_library(
  name = 'chrono',
  hdrs = 'chrono.h',
  srcs = 'chrono.cc',
  deps = [
    ':align',
    '#pthread',
    '#rt',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'chrono_test',
  srcs = 'chrono_test.cc',
  deps = [
    ':chrono',
  ],
  exclusive = True
)

cc_benchmark(
  name = 'chrono_benchmark',
  srcs = 'chrono_benchmark.cc',
  deps = ':chrono',
)

cc_library(
  name = 'string',
  hdrs = 'string.h',
  srcs = 'string.cc',
  deps = [
    '//flare/base/internal:logging',
    '//thirdparty/fmt:fmt',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'string_test',
  srcs = 'string_test.cc',
  deps = [
    ':string',
    ':random',
  ]
)

cc_library(
  name = 'buffer',
  hdrs = 'buffer.h',
  srcs = 'buffer.cc',
  deps = [
    ':object_pool',
    ':ref_ptr',
    '//flare/base/buffer:builtin_buffer_block',
    '//flare/base/buffer:polymorphic_buffer',
    '//flare/base/internal:logging',
    '//flare/base/internal:singly_linked_list',
    '//thirdparty/glog:glog',
  ],
  visibility = 'PUBLIC',
)

for block_size in ['4K', '64K', '1M']:
  cc_test(
    name = 'buffer_test_' + block_size,
    srcs = 'buffer_test.cc',
    deps = [
      ':buffer',
      '//flare/init:override_flag',
      '//flare/testing:main',
    ],
    defs = ['BUFFER_BLOCK_SIZE=\\"%s\\"' % block_size]
  )

cc_library(
  name = 'demangle',
  hdrs = 'demangle.h',
  srcs = 'demangle.cc',
  deps = [
    ':deferred',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'demangle_test',
  srcs = 'demangle_test.cc',
  deps = [
    ':demangle',
  ]
)

cc_library(
  name = 'object_pool',
  hdrs = 'object_pool.h',
  srcs = 'object_pool.cc',
  deps = [
    ':likely',
    ':logging',
    '//flare/base/object_pool:disabled',
    '//flare/base/object_pool:global',
    '//flare/base/object_pool:memory_node_shared',
    '//flare/base/object_pool:thread_local',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'down_cast',
  hdrs = 'down_cast.h',
  srcs = [],
  deps = [
    '//flare/base/internal:logging',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'down_cast_test',
  srcs = 'down_cast_test.cc',
  deps = [':down_cast']
)

cc_library(
  name = 'maybe_owning',
  hdrs = 'maybe_owning.h',
  srcs = [],
  deps = [
    '//flare/base/internal:logging',
    '//thirdparty/googletest:gtest_prod',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'never_destroyed',
  hdrs = 'never_destroyed.h',
  visibility = 'PUBLIC',
)

cc_test(
  name = 'never_destroyed_test',
  srcs = 'never_destroyed_test.cc',
  deps = ':never_destroyed'
)

cc_library(
  name = 'delayed_init',
  hdrs = 'delayed_init.h',
  deps = [
    '//flare/base/internal:logging',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'delayed_init_test',
  srcs = 'delayed_init_test.cc',
  deps = ':delayed_init'
)

cc_library(
  name = 'deferred',
  hdrs = 'deferred.h',
  deps = [':function'],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'deferred_test',
  srcs = 'deferred_test.cc',
  deps = [
    ':deferred',
  ]
)

cc_test(
  name = 'maybe_owning_test',
  srcs = 'maybe_owning_test.cc',
  deps = [':maybe_owning'],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'enum',
  hdrs = 'enum.h',
  visibility = 'PUBLIC',
)

cc_test(
  name = 'enum_test',
  srcs = 'enum_test.cc',
  deps = ':enum',
)

cc_library(
  name = 'ref_ptr',
  hdrs = 'ref_ptr.h',
  deps = [
    '//flare/base/internal:logging',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'ref_ptr_test',
  srcs = 'ref_ptr_test.cc',
  deps = [
    ':chrono',
    ':random',
    ':ref_ptr',
  ]
)

cc_library(
  name = 'erased_ptr',
  hdrs = 'erased_ptr.h',
  srcs = 'erased_ptr.cc',
  deps = [
    ':likely',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'erased_ptr_test',
  srcs = 'erased_ptr_test.cc',
  deps = [
    ':erased_ptr',
  ]
)

cc_library(
  name = 'endian',
  hdrs = 'endian.h',
  visibility = 'PUBLIC',
)

cc_test(
  name = 'endian_test',
  srcs = 'endian_test.cc',
  deps = [
    ':endian',
    ':logging',
  ]
)

cc_library(
  name = 'exposed_var',
  hdrs = 'exposed_var.h',
  srcs = 'exposed_var.cc',
  deps = [
    ':deferred',
    ':tsc',
    ':write_mostly',
    ':string',
    '//flare/base/internal:logging',
    '//flare/base/internal:meta',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'exposed_var_test',
  srcs = 'exposed_var_test.cc',
  deps = [
    ':exposed_var',
    '//flare/testing:main',
    '//thirdparty/jsoncpp:jsoncpp',
  ]
)

cc_library(
  name = 'status',
  hdrs = 'status.h',
  srcs = 'status.cc',
  deps = [
    ':ref_ptr',
    '//flare/base/internal:early_init',
    '//flare/base/internal:logging',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'status_test',
  srcs = 'status_test.cc',
  deps = [
    ':status',
  ]
)

cc_library(
  name = 'casting',
  hdrs = 'casting.h',
  deps = [
    ':demangle',
    '//flare/base/internal:logging',
    '//flare/base/internal:index_alloc',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'casting_test',
  srcs = 'casting_test.cc',
  deps = [
    ':casting',
  ]
)

cc_benchmark(
  name = 'casting_benchmark',
  srcs = 'casting_benchmark.cc',
  deps = ':casting',
)

cc_library(
  name = 'write_mostly',
  hdrs = 'write_mostly.h',
  deps = [
    '//flare/base/write_mostly:basic_ops',
    '//flare/base/write_mostly:metrics',
    '//flare/base/write_mostly:write_mostly',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'id_alloc',
  hdrs = 'id_alloc.h',
  deps = [
    '//flare/base/internal:annotation',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'id_alloc_test',
  srcs = 'id_alloc_test.cc',
  deps = [
    ':chrono',
    ':id_alloc',
    '//flare/base/thread:latch',
  ]
)

cc_benchmark(
  name = 'id_alloc_benchmark',
  srcs = 'id_alloc_benchmark.cc',
  deps = [
    ':id_alloc',
  ]
)

cc_library(
  name = 'function',
  hdrs = 'function.h',
  deps = [
    ':align',
    ':likely',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'function_test',
  srcs = 'function_test.cc',
  deps = ':function'
)

cc_benchmark(
  name = 'function_benchmark',
  srcs = 'function_benchmark.cc',
  deps = ':function',
)

cc_library(
  name = 'function_view',
  hdrs = 'function_view.h',
  deps = [],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'function_view_test',
  srcs = 'function_view_test.cc',
  deps = ':function_view'
)

cc_library(
  name = 'future',
  hdrs = 'future.h',
  deps = '//flare/base/future:future',
  visibility = 'PUBLIC',
)

cc_library(
  name = 'type_index',
  hdrs = 'type_index.h',
  deps = [],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'type_index_test',
  srcs = 'type_index_test.cc',
  deps = ':type_index'
)

cc_library(
  name = 'random',
  hdrs = 'random.h',
  deps = [
    '//flare/base/internal:annotation',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'callback',
  hdrs = 'callback.h',
  deps = [
    '//thirdparty/protobuf:protobuf',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'overloaded',
  hdrs = 'overloaded.h',
  visibility = 'PUBLIC',
)

cc_test(
  name = 'overloaded_test',
  srcs = 'overloaded_test.cc',
  deps = ':overloaded'
)

cc_library(
  name = 'option',
  hdrs = 'option.h',
  srcs = [
    'option.cc',
    'option/dynamically_changed.h',
    'option/gflags_provider.cc',
    'option/option_impl.h',
    'option/option_provider.cc',
    'option/option_provider.h',
    'option/option_service.cc',
    'option/option_service.h',
  ],
  deps = [
    ':dependency_registry',
    ':expected',
    ':function',
    ':logging',
    ':never_destroyed',
    ':string',
    '//flare/base:status',
    '//flare/base/internal:time_keeper',
    '//flare/base/option:key',
    '//flare/base/thread:thread_cached',
    '//flare/init:on_init',
    '//thirdparty/gflags:gflags',
    '//thirdparty/jsoncpp:jsoncpp',
  ],
  link_all_symbols = True,
  visibility = 'PUBLIC',
)

cc_test(
  name = 'option_test',
  srcs = 'option_test.cc',
  deps = [
    ':option',
    '//flare:init',
    '//flare/base/option:json_parser',
    '//thirdparty/gflags:gflags',
  ],
  # Rainbow's SDK does not exit cleanly.
  heap_check = '',
)

cc_benchmark(
  name = 'option_benchmark',
  srcs = 'option_benchmark.cc',
  deps = [
    ':option',
    '//thirdparty/gflags:gflags',
  ]
)

cc_library(
  name = 'monitoring',
  hdrs = 'monitoring.h',
  srcs = [
    'monitoring.cc',
    'monitoring/event.cc',
  ],
  deps = [
    ':chrono',
    ':deferred',
    ':exposed_var',
    ':logging',
    ':string',
    '//flare/base/internal:background_task_host',
    '//flare/base/internal:circular_buffer',
    '//flare/base/internal:cpu',
    '//flare/base/internal:dpc',
    '//flare/base/internal:hash_map',
    '//flare/base/internal:logging',
    '//flare/base/internal:time_keeper',
    '//flare/base/monitoring:dispatcher',
    '//flare/base/monitoring:monitoring_system',
    '//flare/base/thread:out_of_duty_callback',
    '//flare/base/thread:thread_local',
    '//flare/base/thread/thread_local:ref_counted',
    '//thirdparty/gflags:gflags',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'monitoring_test',
  srcs = 'monitoring_test.cc',
  deps = [
    ':down_cast',
    ':monitoring',
    ':random',
    '//flare/base/monitoring:monitoring_system',
    '//flare/fiber:fiber',
    '//flare/init:override_flag',
    '//flare/testing:main',
    '//thirdparty/gflags:gflags',
    '//thirdparty/googletest:gmock',
  ],
  # Heap checker is rather slow that we'd timeout if it's enabled.
  heap_check = '',
)

cc_test(
  name = 'monitoring_with_global_tags_test',
  srcs = 'monitoring_with_global_tags_test.cc',
  deps = [
    ':down_cast',
    ':monitoring',
    ':random',
    '//flare/base/monitoring:monitoring_system',
    '//flare/fiber:fiber',
    '//flare/init:override_flag',
    '//flare/testing:main',
    '//thirdparty/gflags:gflags',
  ],
)

cc_benchmark(
  name = 'monitoring_benchmark',
  srcs = 'monitoring_benchmark.cc',
  deps = ':monitoring'
)

cc_library(
  name = 'compression',
  hdrs = 'compression.h',
  srcs = 'compression.cc',
  deps = [
    '//flare/base/buffer:compression_output_stream',
    '//flare/base/compression:compression',
    '//flare/base/compression:gzip',
    '//flare/base/compression:lz4',
    '//flare/base/compression:snappy',
    '//flare/base/compression:zstd',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'compression_test',
  srcs = 'compression_test.cc',
  deps = [
    ':compression',
  ]
)

cc_library(
  name = 'encoding',
  hdrs = 'encoding.h',
  deps = [
    '//flare/base/encoding:base64',
    '//flare/base/encoding:hex',
    '//flare/base/encoding:percent',
  ],
  visibility = 'PUBLIC',
)

cc_library(
  name = 'expected',
  hdrs = 'expected.h',
  deps = [
    '//flare/base/internal:logging',
    '//flare/base/internal:meta',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'expected_test',
  srcs = 'expected_test.cc',
  deps = ':expected',
)

cc_library(
  name = 'likely',
  hdrs = 'likely.h',
  visibility = 'PUBLIC',
)

cc_library(
  name = 'dependency_registry',
  hdrs = 'dependency_registry.h',
  deps = [
    ':demangle',
    ':function',
    ':maybe_owning',
    ':never_destroyed',
    '//flare/base/internal:hash_map',
    '//flare/base/internal:lazy_init',
    '//flare/base/internal:logging',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'dependency_registry_test',
  srcs = 'dependency_registry_test.cc',
  deps = ':dependency_registry',
)

cc_library(
  name = 'handle',
  hdrs = 'handle.h',
  deps = [
    ':logging',
  ],
  visibility = 'PUBLIC',
)

cc_test(
  name = 'handle_test',
  srcs = 'handle_test.cc',
  deps = ':handle',
)
