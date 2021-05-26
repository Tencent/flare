include('//thirdparty/foreign_build.bld')

cmake_build(
    name = 'lz4_build',
    source_package = 'lz4-1.9.3.tar.gz',
    package_name = 'lz4',
    lib_names = ['lz4'],
    source_dir = 'lz4-1.9.3/build/cmake',
    deps = [
    ],
    cmake_options=[
        '-DBUILD_SHARED_LIBS=ON',
        '-DBUILD_STATIC_LIBS=ON',
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
    ],
    strip_include_prefix='',
    install_dir='',
    lib_dir='lib64',
    generate_dynamic=True,
)

foreign_cc_library(
    name = 'lz4',
    install_dir = '',
    lib_dir='lib64',
    deps = [
      ':lz4_build',
    ],
    visibility = 'PUBLIC',
    has_dynamic=True,
)
