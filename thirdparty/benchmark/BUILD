include('//thirdparty/foreign_build.bld')


cmake_build(
    name='benchmark_build',
    source_package='benchmark-1.5.3.tar.gz',
    package_name='benchmark',
    lib_names=['benchmark','benchmark_main'],
    cmake_options=[
        '-DBENCHMARK_ENABLE_TESTING=OFF',
        '-DCMAKE_BUILD_TYPE=Release',
    ],
    strip_include_prefix='benchmark',
)

foreign_cc_library(
    name='benchmark',
    deps=[
        ':benchmark_build',
        '#pthread',
    ],
    visibility = 'PUBLIC',
)

foreign_cc_library(
    name='benchmark_main',
    deps=[
        ':benchmark_build',
        '#pthread',
    ],
    visibility = 'PUBLIC',
)
