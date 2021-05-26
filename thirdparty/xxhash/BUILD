# Imported from https://github.com/Cyan4973/xxHash

cc_library(
  name = 'xxhash',
  hdrs = 'xxhash.h',
  srcs = [
    'xxhash.c',
  ] + ['xxh_x86dispatch.c'] if build_target.arch == 'x86_64' else [],
  export_incs = '.',
  visibility = 'PUBLIC',
)

cc_test(
  name = 'xxhash_test',
  srcs = 'xxhash_test.cc',
  deps = ':xxhash',
)

cc_binary(
  name = 'xxhsum',
  srcs = 'xxhsum.c',
  deps = ':xxhash',
)
