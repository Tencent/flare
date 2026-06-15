# Build file for the upstream xxHash archive (see //bazel:deps.bzl). Mirrors the
# old in-tree //thirdparty/xxhash target: only XXH64 is used (see
# flare/base/experimental/bloom_filter.cc), so the default dispatch (xxhash.c +
# xxhash.h) is enough. include_prefix keeps the `xxhash/xxhash.h` include path.
cc_library(
    name = "xxhash",
    srcs = ["xxhash.c"],
    hdrs = ["xxhash.h"],
    include_prefix = "xxhash",
    visibility = ["//visibility:public"],
)
