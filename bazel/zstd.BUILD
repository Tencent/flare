cc_library(
    name = "zstd",
    srcs = glob([
        "lib/common/*.c",
        "lib/common/*.h",
        "lib/compress/*.c",
        "lib/compress/*.h",
        "lib/decompress/*.c",
        "lib/decompress/*.h",
        "lib/decompress/*.S",
        "lib/dictBuilder/*.c",
        "lib/dictBuilder/*.h",
    ]),
    hdrs = [
        "lib/zstd.h",
    ],
    include_prefix = "zstd",
    strip_include_prefix = "lib",
    visibility = ["//visibility:public"],
)
