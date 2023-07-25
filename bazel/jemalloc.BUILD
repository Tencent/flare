load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "all",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

configure_make(
    name = "jemalloc",
    configure_options = [
        "--enable-shared",
        "--enable-static",
        "--with-pic",
        "--enable-prof-libunwind",
    ],
    linkopts = [
      "-lpthread",
      "-lrt",
    ],
    autogen = True,
    configure_in_place = True,
    lib_source = ":all",
    out_shared_libs = ["libjemalloc.so"],
    out_static_libs = ["libjemalloc.a"],
)
