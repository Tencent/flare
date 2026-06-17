# Thin wrappers over vcpkg gtest (see BLADE_ROOT vcpkg_config, pinned 1.17.0 to
# match the previously vendored source). Consumers keep depending on
# //thirdparty/googletest:{gtest,gtest_main,gmock,gmock_main}; cc_test_config's
# gtest_libs / gtest_main_libs are unchanged.
#
# linkage='auto' (set in BLADE_ROOT, NO link_all_symbols): gtest's UnitTest
# registry must be a single process-wide instance. dynamic_link tests link the
# shared libgtest, and gmock links *against* that gtest rather than
# whole-archiving a second static copy -- the multiple-registry trap the old
# foreign build documented as "does NOT link in any test case".
cc_library(
    name='gtest',
    hdrs=[],
    deps=['vcpkg#gtest:gtest'],
    visibility='PUBLIC',
)

cc_library(
    name='gtest_main',
    hdrs=[],
    deps=[
        'vcpkg#gtest:gtest_main',
        ':gtest',
    ],
    visibility='PUBLIC',
)

cc_library(
    name='gmock',
    hdrs=[],
    deps=[
        'vcpkg#gtest:gmock',
        ':gtest',
    ],
    visibility='PUBLIC',
)

cc_library(
    name='gmock_main',
    hdrs=[],
    deps=[
        'vcpkg#gtest:gmock_main',
        ':gmock',
    ],
    visibility='PUBLIC',
)

# Production code includes "gtest/gtest_prod.h" (the FRIEND_TEST macro) without
# linking gtest, so this stays a standalone committed header -- never a lib dep.
cc_library(
    name='gtest_prod',
    hdrs='gtest/gtest_prod.h',
    visibility='PUBLIC',
    export_incs='.',
)
