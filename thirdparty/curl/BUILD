include('//thirdparty/foreign_build.bld')

_SSL_PATH = get_install_dir('openssl')
_NGHTTP2_PATH = get_install_dir('nghttp2')
_ZLIB_PATH = get_install_dir('zlib')

autotools_build(
    name='curl_build',
    source_package='curl-7.71.0.tar.bz2',
    package_name='curl',
    lib_names=['curl'],
    configure_options=' '.join([
        '--with-nghttp2=' + _NGHTTP2_PATH,
        '--with-ssl=' + _SSL_PATH,
        '--with-zlib=' + _ZLIB_PATH,
        '--enable-shared',
        '--enable-static',
        '--with-pic',
        '--disable-ldap',
        '--disable-ldaps',
        '--without-brotli',
    ]),
    deps=[
        '//thirdparty/nghttp2:nghttp2',
        '//thirdparty/openssl:crypto',
        '//thirdparty/openssl:ssl',
        '//thirdparty/zlib:z',
    ],
    ld_library_path=[_SSL_PATH + '/lib', _NGHTTP2_PATH + '/lib'],
    strip_include_prefix='curl',
    generate_dynamic=True,
)

foreign_cc_library(
    name='curl',
    deps=[
        ':curl_build',
        '//thirdparty/nghttp2:nghttp2',
        '//thirdparty/openssl:crypto',
        '//thirdparty/openssl:ssl',
        '//thirdparty/zlib:z',
        '#pthread',
        '#rt',
    ],
    visibility='PUBLIC',
    has_dynamic=True,
)
