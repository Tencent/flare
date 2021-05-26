# Imported from: https://github.com/BLAKE3-team/BLAKE3/tree/master/c

cc_library(
  name = 'blake3',
  hdrs = 'blake3.h',
  srcs = [
    'blake3.c',
    'blake3_dispatch.c',
    'blake3_portable.c',
    'blake3_impl.h',
  ] + ([
    'blake3_sse2_x86-64_unix.S',
    'blake3_sse41_x86-64_unix.S',
    'blake3_avx2_x86-64_unix.S',
    'blake3_avx512_x86-64_unix.S',
  ] if build_target.arch == 'x86_64' else [
    'blake3_neon.c',
  ] if build_target.arch == 'aarch64' else []),
  extra_cppflags = ['-O3'],  # As suggested by `Makefile.testing`
  export_incs = '.',
  warning = 'no',
  visibility = 'PUBLIC',
)

cc_test(
  name = 'blake3_test',
  srcs = 'blake3_test.cc',
  deps = [
    ':blake3',
  ]
)
