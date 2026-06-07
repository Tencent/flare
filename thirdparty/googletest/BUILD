include('//thirdparty/foreign_build.bld')

cmake_build(
    name='googletest_build',
    source_package='googletest-1.17.0.tar.gz',
    package_name='googletest',
    lib_names=['gmock', 'gmock_main', 'gtest', 'gtest_main'],
    cmake_options=[
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_CXX_FLAGS=-Wno-deprecated-copy',
    ],
    # googletest's CMake emits either static or shared per configure (it keys
    # solely off BUILD_SHARED_LIBS), so a single pass can't produce both. The
    # static pass above yields libgtest.a (used by static_link consumers); this
    # shared pass yields libgtest.dylib. Release builds default to dynamic_link
    # (BLADE_ROOT), so cc_tests link the dylib -- giving gtest's UnitTest
    # registry a single process-wide instance. Without a shared gtest, the
    # static archive gets force_loaded (link_all_symbols) into every mock/main
    # dylib, producing multiple registries where TEST() registers into one and
    # RUN_ALL_TESTS() reads another ("does NOT link in any test case"). Using
    # BUILD_SHARED_LIBS directly means the stale generate_dynamic.patch (written
    # for 1.10) is no longer needed.
    shared_cmake_options=['-DBUILD_SHARED_LIBS=ON'],
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
