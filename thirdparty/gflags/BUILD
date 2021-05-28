include('../foreign_build.bld')

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
    ],
    strip_include_prefix = 'gflags',
    generate_dynamic=True,
)

foreign_cc_library(
    name='gflags',
    deps=[
        ':gflags_build',
        '#pthread',
        '#rt',
        '#z',
    ],
    has_dynamic=True,
    visibility = 'PUBLIC',
)

cc_test(
    name='gflags_test',
    srcs='gflags_test.cpp',
    deps=':gflags',
)
