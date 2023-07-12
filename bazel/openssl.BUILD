load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "all",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

configure_make(
    name = "openssl",
    configure_command = "config",
    configure_in_place = True,
    configure_options = [
        "--libdir=lib"
    ],
    out_binaries = ["openssl"],
    lib_source = ":all",
    out_shared_libs = [
        "libssl.so",
        "libcrypto.so",
    ],
    out_static_libs = [
        "libssl.a",
        "libcrypto.a",
    ],
    targets = [
        "build_libs",
        "install_sw",
    ],
)

cc_library(
    name = "crypto",
    deps = [":openssl"],
    linkopts = [
        "-ldl",
        "-lpthread",
    ]
)

cc_library(
    name = "ssl",
    deps = [
        ":openssl",
        ":crypto",
    ],
)
