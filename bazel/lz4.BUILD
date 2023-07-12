cc_library(
    name = "lz4",
    srcs = [
        "lib/lz4.c",
        "lib/lz4frame.c",
        "lib/lz4frame_static.h",
        "lib/lz4hc.c",
        "lib/xxhash.c",
        "lib/xxhash.h",
    ],
    hdrs = [
        "lib/lz4.h",
        "lib/lz4frame.h",
        "lib/lz4hc.h",
    ],
    include_prefix = "lz4",
    strip_include_prefix = "lib/",
    textual_hdrs = ["lib/lz4.c"],
    visibility = ["//visibility:public"],
)
