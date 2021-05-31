include('../foreign_build.bld')

_GFLAGS_PATH = get_install_dir('gflags')
_ZLIB_PATH = get_install_dir('zlib')

autotools_build(
    name='gperftools_build',
    source_package='gperftools-2.8.tar.gz',
    package_name='gperftools',
    lib_names=[
        'profiler', 'tcmalloc', 'tcmalloc_and_profiler', 'tcmalloc_debug',
        'tcmalloc_minimal', 'tcmalloc_minimal_debug'
    ],
    configure_options=' '.join([
        '--with-gflags=' + _GFLAGS_PATH, '--with-zlib=' + _ZLIB_PATH,
        '--enable-shared', '--enable-static', '--with-pic',
        '--enable-heap-checker', '--enable-heap-profiler',
        '--enable-cpu-profiler', '--enable-debugalloc',
        '--enable-frame-pointers'
    ]),
    deps=[
        '//thirdparty/gflags:gflags',
        '//thirdparty/zlib:z',
    ],
    strip_include_prefix='gperftools',
    configure_file_name='./configure',
    generate_dynamic=True,
)

foreign_cc_library(
    name='profiler',
    deps=[
        ':gperftools_build',
        '//thirdparty/zlib:z',
        '#pthread',
        '#rt',
    ],
    export_incs='//' + blade.path.join(blade.current_target_dir(), 'include'),
    has_dynamic=True,
    visibility='PUBLIC',
)

foreign_cc_library(
    name='tcmalloc',
    deps=[
        ':gperftools_build',
        '//thirdparty/zlib:z',
        '#pthread',
        '#rt',
    ],
    export_incs='//' + blade.path.join(blade.current_target_dir(), 'include'),
    has_dynamic=True,
    visibility='PUBLIC',
)

foreign_cc_library(
    name='tcmalloc_and_profiler',
    deps=[
        ':gperftools_build',
        '//thirdparty/zlib:z',
        '#pthread',
        '#rt',
    ],
    export_incs='//' + blade.path.join(blade.current_target_dir(), 'include'),
    has_dynamic=True,
    visibility='PUBLIC',
)

foreign_cc_library(
    name='tcmalloc_debug',
    deps=[
        ':gperftools_build',
        '//thirdparty/zlib:z',
        '#pthread',
        '#rt',
    ],
    export_incs='//' + blade.path.join(blade.current_target_dir(), 'include'),
    has_dynamic=True,
    visibility='PUBLIC',
)

foreign_cc_library(
    name='tcmalloc_minimal',
    deps=[
        ':gperftools_build',
        '//thirdparty/zlib:z',
        '#pthread',
        '#rt',
    ],
    export_incs='//' + blade.path.join(blade.current_target_dir(), 'include'),
    has_dynamic=True,
    visibility='PUBLIC',
)

foreign_cc_library(
    name='tcmalloc_minimal_debug',
    deps=[
        ':gperftools_build',
        '//thirdparty/zlib:z',
        '#pthread',
        '#rt',
    ],
    export_incs='//' + blade.path.join(blade.current_target_dir(), 'include'),
    has_dynamic=True,
    visibility='PUBLIC',
)
