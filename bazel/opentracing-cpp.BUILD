# This file is based on https://github.com/opentracing/opentracing-cpp/blob/master/BUILD.bazel

# Change the header file prefix opentracing to opentracing-cpp,
# and no longer call cmake to generate version information,
# which is managed by bazel

cc_library(
    name = "opentracing",
    srcs = glob(
        ["src/**/*.cpp"],
        exclude = [
            "src/dynamic_load_unsupported.cpp",
            "src/dynamic_load_windows.cpp",
        ],
    ),
    hdrs = glob(["include/opentracing/**/*.h"]) + [
        ":include/opentracing/config.h",
        ":include/opentracing/version.h",
    ],
    include_prefix = "opentracing-cpp",
    includes = ["include"],
    linkopts = [
        "-ldl",
    ],
    strip_include_prefix = "include/opentracing",
    visibility = ["//visibility:public"],
    deps = [
        "//3rd_party:expected",
        "//3rd_party:variant",
    ],
)

genrule(
    name = "config_h",
    outs = ["include/opentracing/config.h"],
    cmd = """cat <<EOF >$@
#pragma once

#define OPENTRACING_BUILD_DYNAMIC_LOADING
EOF
""",
)

genrule(
    name = "version_h",
    outs = ["include/opentracing/version.h"],
    cmd = """cat <<EOF >$@
#ifndef OPENTRACING_VERSION_H
#define OPENTRACING_VERSION_H

#define OPENTRACING_VERSION "1.5.1"
#define OPENTRACING_ABI_VERSION "2"

// clang-format off
#define BEGIN_OPENTRACING_ABI_NAMESPACE \
  inline namespace v2 {
#define END_OPENTRACING_ABI_NAMESPACE \
  }  // namespace v2
// clang-format on

#endif // OPENTRACING_VERSION_H
EOF
""",
)
