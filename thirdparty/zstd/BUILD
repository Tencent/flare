include('//thirdparty/foreign_build.bld')

cmake_build(
    name = 'zstd_build',
    source_package = 'zstd-1.4.5.tar.gz',
    package_name = 'zstd',
    lib_names = ['zstd'],
    source_dir = 'zstd-1.4.5/build/cmake',
    cmake_options=[
        '-DZSTD_BUILD_SHARED=ON',
        '-DZSTD_BUILD_STATIC=ON',
        '-DCMAKE_BUILD_TYPE=Release',
    ],
    generate_dynamic=True,
)

foreign_cc_library(
    name = 'zstd',
    deps = [
      ':zstd_build',
    ],
    visibility = 'PUBLIC',
    has_dynamic=True,
)
