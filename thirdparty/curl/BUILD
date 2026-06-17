# curl via vcpkg (vcpkg#curl:curl, baseline in BLADE_ROOT; openssl + http2
# features). flare includes "curl/<h>" (vcpkg native include/curl/ layout). curl
# links its TLS/http2/zlib deps from the same vcpkg install; declare them
# explicitly so a static curl's references resolve.
_is_linux = blade.cc_toolchain.target_os == 'linux'

cc_library(
    name='curl',
    hdrs=[],
    deps=[
        'vcpkg#curl:curl',
        '//thirdparty/openssl:ssl',
        '//thirdparty/openssl:crypto',
        '//thirdparty/nghttp2:nghttp2',
        '//thirdparty/zlib:z',
    ] + (['#pthread', '#rt'] if _is_linux else []),
    visibility='PUBLIC',
)
