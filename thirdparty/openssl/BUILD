# openssl via vcpkg (vcpkg#openssl:{crypto,ssl}, 3.x baseline in BLADE_ROOT).
# Thin wrappers keep //thirdparty/openssl:{crypto,ssl} for consumers; flare
# includes "openssl/<h>" (vcpkg native include/openssl/ layout).
_is_linux = blade.cc_toolchain.target_os == 'linux'

cc_library(
    name='crypto',
    hdrs=[],
    deps=['vcpkg#openssl:crypto'] + (['#dl', '#pthread'] if _is_linux else []),
    visibility='PUBLIC',
)

cc_library(
    name='ssl',
    hdrs=[],
    deps=['vcpkg#openssl:ssl', ':crypto'],
    visibility='PUBLIC',
)
