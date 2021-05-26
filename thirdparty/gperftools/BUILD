include('../foreign_build.bld')

_GFLAGS_PATH = blade.path.join(
    blade.path.dirname(blade.path.abspath(blade.current_target_dir())), 'gflags')

autotools_build(
    name='gperftools_build',
    source_package='gperftools-2.8.tar.gz',
    package_name='gperftools',
    lib_names=[
        'profiler', 'tcmalloc', 'tcmalloc_and_profiler', 'tcmalloc_debug',
        'tcmalloc_minimal', 'tcmalloc_minimal_debug'
    ],
    configure_options=' '.join([
        '--with-gflags=' + _GFLAGS_PATH, ' --enable-shared', '--enable-static',
        '--with-pic', '--enable-heap-checker', '--enable-heap-profiler',
        '--enable-cpu-profiler', '--enable-debugalloc',
        '--enable-frame-pointers'
    ]),
    deps=[
        '//thirdparty/gflags:gflags',
    ],
    strip_include_prefix='gperftools',
    configure_file_name='./configure',
    generate_dynamic=True,
)

foreign_cc_library(
    name='profiler',
    install_dir='',
    deps=[
        ':gperftools_build',
        '#pthread',
        '#rt',
        '#z',
    ],
    export_incs='//' + blade.path.join(blade.current_target_dir(), 'include'),
    has_dynamic=True,
    visibility = 'PUBLIC',
)

foreign_cc_library(
    name='tcmalloc',
    install_dir='',
    deps=[
        ':gperftools_build',
        '#pthread',
        '#rt',
        '#z',
    ],
    export_incs='//' + blade.path.join(blade.current_target_dir(), 'include'),
    has_dynamic=True,
    visibility = 'PUBLIC',
)

foreign_cc_library(
    name='tcmalloc_and_profiler',
    install_dir='',
    deps=[
        ':gperftools_build',
        '#pthread',
        '#rt',
        '#z',
    ],
    export_incs='//' + blade.path.join(blade.current_target_dir(), 'include'),
    has_dynamic=True,
    visibility = 'PUBLIC',
)

foreign_cc_library(
    name='tcmalloc_debug',
    install_dir='',
    deps=[
        ':gperftools_build',
        '#pthread',
        '#rt',
        '#z',
    ],
    export_incs='//' + blade.path.join(blade.current_target_dir(), 'include'),
    has_dynamic=True,
    visibility = 'PUBLIC',
)

foreign_cc_library(
    name='tcmalloc_minimal',
    install_dir='',
    deps=[
        ':gperftools_build',
        '#pthread',
        '#rt',
        '#z',
    ],
    export_incs='//' + blade.path.join(blade.current_target_dir(), 'include'),
    has_dynamic=True,
    visibility = 'PUBLIC',
)

foreign_cc_library(
    name='tcmalloc_minimal_debug',
    install_dir='',
    deps=[
        ':gperftools_build',
        '#pthread',
        '#rt',
        '#z',
    ],
    export_incs='//' + blade.path.join(blade.current_target_dir(), 'include'),
    has_dynamic=True,
    visibility = 'PUBLIC',
)
