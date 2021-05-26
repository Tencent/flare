include('//thirdparty/foreign_build.bld')

_SSL_PATH = blade.path.join(blade.path.dirname(blade.path.abspath(blade.current_target_dir())), 'openssl')
_NGHTTP2_PATH = blade.path.join(blade.path.dirname(blade.path.abspath(blade.current_target_dir())), 'nghttp2')

autotools_build(
    name = 'curl_build',
    source_package = 'curl-7.71.0.tar.bz2',
    package_name = 'curl',
    lib_names = ['curl'],
    configure_options = '--with-nghttp2=' + _NGHTTP2_PATH + ' --with-ssl=' + _SSL_PATH + ' --enable-shared --enable-static --with-pic  --disable-ldap --disable-ldaps',
    deps = [
      '//thirdparty/nghttp2:nghttp2',
      '//thirdparty/openssl:crypto',
      '//thirdparty/openssl:ssl',
    ],
    ld_library_path = [_SSL_PATH + '/lib', _NGHTTP2_PATH + '/lib'],
    strip_include_prefix='curl',
    generate_dynamic=True,
)

foreign_cc_library(
    name = 'curl',
    install_dir = '',
    deps=[
      ':curl_build',
      '#pthread',
      '#rt',
      '#z',
      '//thirdparty/openssl:crypto',
      '//thirdparty/openssl:ssl',
      '//thirdparty/nghttp2:nghttp2',
    ],
    visibility = 'PUBLIC',
    has_dynamic=True,
)
