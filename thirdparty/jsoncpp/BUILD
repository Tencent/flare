include('//thirdparty/foreign_build.bld')


cmake_build(
    name='jsoncpp_build',
    source_package='jsoncpp-0.10.7.tar.gz',
    package_name='jsoncpp',
    lib_names=['jsoncpp'],
    cmake_options=[
        '-DBUILD_SHARED_LIBS=ON',
        '-DBUILD_STATIC_LIBS=ON',
        '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
    ],
    strip_include_prefix='jsoncpp/json',
    generate_dynamic=True,
)

foreign_cc_library(
    name='jsoncpp',
    deps=[
        ':jsoncpp_build',
        '#pthread',
    ],
    visibility = 'PUBLIC',
    has_dynamic=True,
)
