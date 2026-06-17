# Thin wrappers over vcpkg google/benchmark (pinned to 1.7.1 in BLADE_ROOT).
# Consumers keep depending on //thirdparty/benchmark:{benchmark,benchmark_main}.
cc_library(
    name='benchmark',
    hdrs=[],
    deps=['vcpkg#benchmark:benchmark'],
    visibility='PUBLIC',
)

cc_library(
    name='benchmark_main',
    hdrs=[],
    deps=[
        'vcpkg#benchmark:benchmark_main',
        ':benchmark',
    ],
    visibility='PUBLIC',
)
