cc_library(
    name = "fmt",
    srcs = [
        #"src/fmt.cc", # No C++ module support
        "src/format.cc",
        "src/os.cc",
    ],
    hdrs = glob(["include/fmt/*.h"]),
    includes = [
        "include",
    ],
    strip_include_prefix = "include",
    visibility = ["//visibility:public"],
)
