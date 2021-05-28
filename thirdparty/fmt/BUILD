include('//thirdparty/foreign_build.bld')

cmake_build(
    name='fmt_build',
    source_package='fmt-7.1.3.tar.gz',
    package_name='fmt',
    lib_names=['fmt'],
    cmake_options=[
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        '-DCMAKE_BUILD_TYPE=Release',
        '-DFMT_TEST=OFF',
    ],
    strip_include_prefix='fmt',
    patches=['generate_dynamic.patch'],
    generate_dynamic=True,
)

foreign_cc_library(
    name='fmt',
    deps=[
        ':fmt_build',
    ],
    visibility='PUBLIC',
    has_dynamic=True,
)
