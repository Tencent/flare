load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "all",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

configure_make(
    name = "nghttp2",
    configure_options = [
        "--enable-lib-only",
        "--enable-shared",
        "--enable-static",
        "--with-pic",
    ],
    lib_source = ":all",
    # out_shared_libs = ["libnghttp2.so"],
    out_static_libs = ["libnghttp2.a"],
)
