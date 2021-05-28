include('//thirdparty/foreign_build.bld')

cmake_build(
    name='opentracing-cpp_build',
    source_package='opentracing-cpp-1.5.1.tar.gz',
    package_name='opentracing-cpp',
    lib_names=['opentracing', 'opentracing_mocktracer'],
    cmake_options=[
        '-DBUILD_SHARED_LIBS=ON',
        '-DBUILD_STATIC_LIBS=ON',
        '-DBUILD_MOCKTRACER=ON',
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        '-DCMAKE_BUILD_TYPE=Release',
    ],
    strip_include_prefix='opentracing',
    generate_dynamic=True,
)

foreign_cc_library(
    name='opentracing',
    deps=[
        ':opentracing-cpp_build',
    ],
    visibility='PUBLIC',
    has_dynamic=True,
)

foreign_cc_library(
    name='opentracing_mocktracer',
    deps=[
        ':opentracing-cpp_build',
    ],
    visibility='PUBLIC',
    has_dynamic=True,
)
