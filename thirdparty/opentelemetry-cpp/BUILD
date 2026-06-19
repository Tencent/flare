# opentelemetry-cpp (API only) via vcpkg (vcpkg#opentelemetry-cpp, pinned by the
# baseline in BLADE_ROOT). flare uses only the header-only `opentelemetry_api`
# component for distributed tracing (no SDK / exporters). This thin wrapper
# keeps the //thirdparty/opentelemetry-cpp:api reference stable for consumers.
cc_library(
    name = 'api',
    hdrs = [],
    # The `opentelemetry_api` component is header-only (no `.a`), so blade can't
    # reference it as a library. `opentelemetry_common` is the smallest real lib
    # in the install; depending on it gives us the include dir (which holds both
    # `opentelemetry/` and `absl/` headers) and links cleanly. We still consume
    # only the API headers -- no SDK/exporter (and thus no protobuf/grpc).
    deps = ['vcpkg#opentelemetry-cpp:opentelemetry_common'],
    visibility = 'PUBLIC',
)
