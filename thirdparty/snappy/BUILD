include('//thirdparty/foreign_build.bld')

cmake_build(name='snappy_build',
            source_package='snappy-1.1.8.tar.gz',
            package_name='snappy',
            lib_names=['snappy'],
            cmake_options=[
                '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
                '-DCMAKE_BUILD_TYPE=Release',
                '-DSNAPPY_BUILD_TESTS=OFF',
                '-DZLIB_ROOT=' + get_install_dir('zlib'),
            ],
            deps=[
                '//thirdparty/zlib:z',
            ],
            patches=['generate_dynamic.patch'],
            generate_dynamic=True)

foreign_cc_library(
    name='snappy',
    deps=[
        ':snappy_build',
        '//thirdparty/zlib:z',
        '#rt',
    ],
    visibility='PUBLIC',
    has_dynamic=True,
)
