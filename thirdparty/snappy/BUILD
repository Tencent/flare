# snappy via vcpkg (vcpkg#snappy:snappy). //thirdparty/snappy:snappy unchanged.
_is_linux = blade.cc_toolchain.target_os == 'linux'
cc_library(name='snappy', hdrs=[],
           deps=['vcpkg#snappy:snappy'] + (['#rt'] if _is_linux else []),
           visibility='PUBLIC')
