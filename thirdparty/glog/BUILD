include('../foreign_build.bld')

_GFLAGS_PATH = get_install_dir('gflags')
_ZLIB_PATH = get_install_dir('zlib')

autotools_build(
    name='glog_build',
    source_package='glog-0.4.0.tar.gz',
    package_name='glog',
    lib_names=['glog'],
    configure_options=' '.join([
        '--with-gflags=' + _GFLAGS_PATH,
        '--with-zlib=' + _ZLIB_PATH,
        '--enable-shared',
        '--enable-static',
        '--with-pic',
    ]),
    deps=[
        '//thirdparty/gflags:gflags',
        '//thirdparty/zlib:z',
    ],
    strip_include_prefix='glog',
    configure_file_name='./autogen.sh && ./configure',
    generate_dynamic=True,
)

foreign_cc_library(
    name='glog',
    deps=[
        ':glog_build',
        '//thirdparty/zlib:z',
        '#pthread',
        '#rt',
    ],
    has_dynamic=True,
    visibility='PUBLIC',
)

cc_test(name='glog_test',
        srcs='glog_test.cpp',
        deps=[
            ':glog',
            '//thirdparty/gflags:gflags',
            '#pthread',
        ])

cc_binary(name='press',
          srcs='press.cpp',
          deps=[
              ':glog',
              '//thirdparty/gflags:gflags',
              '#pthread',
          ])
