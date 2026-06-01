include('../foreign_build.bld')

_is_linux = blade.cc_toolchain.target_os == 'linux'

cmake_build(
    name='gflags_build',
    package_name='gflags',
    source_package='gflags-2.2.2.tar.gz',
    source_dir='gflags-2.2.2',
    lib_names=['gflags'],
    cmake_options=[
        '-DGFLAGS_BUILD_SHARED_LIBS=ON',
        '-DGFLAGS_BUILD_STATIC_LIBS=ON',
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        '-DCMAKE_BUILD_TYPE=Release',
        # Disabling `CMAKE_EXPORT_PACKAGE_REGISTRY` does not work for gflags, we
        # need to suppress the behaviro using gflags' own flag.
        '-DREGISTER_BUILD_DIR=OFF',
        '-DREGISTER_INSTALL_PREFIX=OFF'
    ],
    strip_include_prefix='gflags',
    generate_dynamic=True,
)

foreign_cc_library(
    name='gflags',
    deps=[
        ':gflags_build',
    ] + (['#pthread', '#rt'] if _is_linux else []),
    # gflags must expose its .so (not just .a) so that downstream consumers all
    # link a single instance — without this, libglog.{so,dylib} embeds gflags
    # while exes also link libgflags.a directly, causing duplicate flag
    # registration at startup.
    has_dynamic=True,
    visibility='PUBLIC',
)

cc_test(
    name='gflags_test',
    srcs='gflags_test.cpp',
    deps=':gflags',
)
