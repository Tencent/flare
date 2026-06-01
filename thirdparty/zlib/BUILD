include('//thirdparty/foreign_build.bld')

cmake_build(name='zlib_build',
            source_package='zlib-1.3.2.tar.gz',
            package_name='zlib',
            lib_names=['z'],
            cmake_options=[
                '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
                '-DCMAKE_BUILD_TYPE=Release',
            ],
            generate_dynamic=True)

foreign_cc_library(
    name='z',
    deps=[
        ':zlib_build',
    ],
    visibility='PUBLIC',
    has_dynamic=True,
)
