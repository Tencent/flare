include('//thirdparty/foreign_build.bld')

cmake_build(
    name='snappy_build',
    source_package='snappy-1.1.8.tar.gz',
    package_name='snappy',
    lib_names=['snappy'],
    # FIXME: How can we both generates a static library and a dynamic library?
    cmake_options=[
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        '-DCMAKE_BUILD_TYPE=Release',
        '-DSNAPPY_BUILD_TESTS=OFF',
    ],
    patches=['generate_dynamic.patch'],
    generate_dynamic=True)

foreign_cc_library(
    name='snappy',
    install_dir='',
    lib_dir='lib64',
    deps=[
        ':snappy_build',
        '#rt',
        '#z',
    ],
    visibility='PUBLIC',
    has_dynamic=True,
)
