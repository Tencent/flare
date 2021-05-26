include('//thirdparty/foreign_build.bld')

autotools_build(
    name = 'nghttp2_build',
    source_package = 'nghttp2-1.41.0.tar.bz2',
    package_name = 'nghttp2',
    lib_names = ['nghttp2'],
    configure_options = '--enable-lib-only --enable-shared --enable-static --with-pic',
    strip_include_prefix = 'nghttp2',
    generate_dynamic=True,
)

foreign_cc_library(
    name = 'nghttp2',
    install_dir = '',
    deps = [
      ':nghttp2_build',
    ],
    visibility = 'PUBLIC',
    has_dynamic=True,
)
