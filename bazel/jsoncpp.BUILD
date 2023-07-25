cc_library(
    name = "jsoncpp",
    srcs = [
        "src/lib_json/json_reader.cpp",
        "src/lib_json/json_tool.h",
        "src/lib_json/json_value.cpp",
        "src/lib_json/json_writer.cpp",
    ],
    hdrs = glob(["include/json/*.h"]),
    copts = [
        "-DJSON_USE_EXCEPTION=0",
        "-DJSON_HAS_INT64",
    ],
    include_prefix = "jsoncpp",
    includes = ["include"],
    strip_include_prefix = "include/json",
    visibility = ["//visibility:public"],
    deps = [":private"],
)

cc_library(
    name = "private",
    textual_hdrs = ["src/lib_json/json_valueiterator.inl"],
)
