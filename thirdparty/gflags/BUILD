# gflags is provided by vcpkg (vcpkg#gflags:gflags, pinned to 2.2.2 and built
# shared in BLADE_ROOT -- a single instance avoids duplicate flag registration).
# This thin wrapper keeps the //thirdparty/gflags:gflags reference unchanged.
_is_linux = blade.cc_toolchain.target_os == 'linux'

cc_library(
    name = 'gflags',
    hdrs = [],
    deps = ['vcpkg#gflags:gflags'] + (['#pthread', '#rt'] if _is_linux else []),
    visibility = 'PUBLIC',
)
