include('//thirdparty/foreign_build.bld')


cmake_build(
    name='yaml-cpp_build',
    source_package='yaml-cpp-yaml-cpp-0.6.3.tar.gz',
    package_name='yaml-cpp',
    lib_names=['yaml-cpp'],
    deps=[],
    cmake_options=[
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        '-DCMAKE_BUILD_TYPE=Release',
        '-DYAML_CPP_BUILD_TESTS=OFF',
    ],
    install_dir='',
    strip_include_prefix='yaml-cpp',
    lib_dir='lib',
    patches=['generate_dynamic.patch']
)

foreign_cc_library(
    name='yaml-cpp',
    install_dir='',
    lib_dir='lib',
    deps=[
        ':yaml-cpp_build',
    ],
    visibility = 'PUBLIC',
)

# For compatibility reasons.
cc_library(
    name = 'yaml_cpp',
    hdrs = 'yaml.h',
    deps = ':yaml-cpp',
    visibility = 'PUBLIC',
)
