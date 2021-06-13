include('//thirdparty/foreign_build.bld')

cmake_build(
    name='googletest_build',
    source_package='googletest-release-1.10.0.tar.gz',
    package_name='googletest',
    lib_names=['gmock', 'gmock_main', 'gtest', 'gtest_main'],
    cmake_options=[
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        '-DCMAKE_BUILD_TYPE=Release',
    ],
    patches=['generate_dynamic.patch'],
    generate_dynamic=True,
)

foreign_cc_library(
    name='gmock',
    deps=[
        ':gtest',
        ':googletest_build',
    ],
    visibility='PUBLIC',
    has_dynamic=True,
)

foreign_cc_library(
    name='gmock_main',
    deps=[
        ':gmock',
        ':googletest_build',
    ],
    visibility='PUBLIC',
    has_dynamic=True,
)

foreign_cc_library(
    name='gtest',
    deps=[
        ':googletest_build',
    ],
    visibility='PUBLIC',
    has_dynamic=True,
)

foreign_cc_library(
    name='gtest_main',
    deps=[
        ':gtest',
        ':googletest_build',
    ],
    visibility='PUBLIC',
    has_dynamic=True,
)

cc_library(
    name='gtest_prod',
    hdrs='gtest/gtest_prod.h',
    visibility='PUBLIC',
    export_incs='.',
)
