# opentelemetry-cpp, API only. flare uses just the header-only tracing API (no
# SDK / exporters, hence no protobuf / grpc). With neither `HAVE_ABSEIL` nor an
# `OPENTELEMETRY_STL_VERSION` defined, the `nostd::` types use opentelemetry's
# own bundled, self-contained implementations -- so this is purely header-only
# with no external dependencies.

cc_library(
    name = "api",
    hdrs = glob(["api/include/**/*.h"]),
    includes = ["api/include"],
    visibility = ["//visibility:public"],
)
