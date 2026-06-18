# openssl via vcpkg (vcpkg#openssl:{crypto,ssl}, 3.x baseline in BLADE_ROOT).
# Thin wrappers keep //thirdparty/openssl:{crypto,ssl} for consumers; flare
# includes "openssl/<h>" (vcpkg native include/openssl/ layout).
#
# No #dl/#pthread here: blade pulls libcrypto's system libs from its pkg-config
# (Libs.private: -ldl) automatically, and flare links pthread globally on Linux.
cc_library(
    name='crypto',
    hdrs=[],
    deps=['vcpkg#openssl:crypto'],
    visibility='PUBLIC',
)

cc_library(
    name='ssl',
    hdrs=[],
    deps=['vcpkg#openssl:ssl', ':crypto'],
    visibility='PUBLIC',
)
