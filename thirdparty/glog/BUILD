include('../foreign_build.bld')

_GFLAGS_PATH = blade.path.join(
    blade.path.dirname(blade.path.abspath(blade.current_target_dir())),
    'gflags')

autotools_build(
    name='glog_build',
    source_package='glog-0.4.0.tar.gz',
    package_name='glog',
    lib_names=['glog'],
    configure_options='--with-gflags=' + _GFLAGS_PATH +
    ' --enable-shared --enable-static --with-pic',
    deps=[
        '//thirdparty/gflags:gflags',
    ],
    strip_include_prefix='glog',
    configure_file_name='./autogen.sh && ./configure',
    generate_dynamic=True,
)

foreign_cc_library(
    name='glog',
    deps=[
        ':glog_build',
        '#pthread',
        '#rt',
        '#z',
    ],
    has_dynamic=True,
    visibility = 'PUBLIC',
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
