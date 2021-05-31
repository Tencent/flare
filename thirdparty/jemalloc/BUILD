include('../foreign_build.bld')

autotools_build(
    name='jemalloc_build',
    source_package='jemalloc-5.2.1.tar.gz',
    package_name='jemalloc',
    lib_names=['jemalloc'],
    configure_options=' '.join([
        '--enable-shared', '--enable-static', '--with-pic',
        '--enable-prof-libunwind'
    ]),
    strip_include_prefix='jemalloc',
    configure_file_name='./autogen.sh && ./configure',
    generate_dynamic=True,
)

foreign_cc_library(
    name='jemalloc',
    deps=[
        ':jemalloc_build',
        '#pthread',
        '#rt',
    ],
    has_dynamic=True,
    visibility='PUBLIC',
)
