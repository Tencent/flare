include('../foreign_build.bld')

autotools_build(
    name='ctemplate_build',
    source_package='ctemplate-ctemplate-2.4.tar.gz',
    package_name='ctemplate',
    lib_names=['ctemplate'],
    configure_options=' '.join([
        '--enable-shared', '--enable-static', '--with-pic',
    ]),
    strip_include_prefix='ctemplate',
    configure_file_name='./autogen.sh && ./configure',
    generate_dynamic=True,
)

foreign_cc_library(
    name='ctemplate',
    install_dir='',
    deps=[
        ':ctemplate_build',
    ],
    has_dynamic=True,
    visibility = 'PUBLIC',
)
