# Thin wrapper over vcpkg yaml-cpp (see BLADE_ROOT vcpkg_config). Consumers keep
# depending on //thirdparty/yaml-cpp:yaml-cpp (or the :yaml_cpp compat alias).
cc_library(
    name='yaml-cpp',
    hdrs=[],
    deps=['vcpkg#yaml-cpp:yaml-cpp'],
    visibility='PUBLIC',
)

# For compatibility reasons (the underscore-named alias). Consumers include
# "yaml-cpp/yaml.h" via the dep, so no own hdrs.
cc_library(
    name='yaml_cpp',
    hdrs=[],
    deps=[':yaml-cpp'],
    visibility='PUBLIC',
)
