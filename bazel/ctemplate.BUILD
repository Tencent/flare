load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "all",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

configure_make(
    name = "ctemplate",
    autogen = True,
    configure_in_place = True,
    configure_options = [
        "--enable-shared",
        "--enable-static",
        "--with-pic",
    ],
    lib_source = ":all",
    # out_shared_libs = ["libctemplate.so"],
    out_static_libs = ["libctemplate.a"],
)
