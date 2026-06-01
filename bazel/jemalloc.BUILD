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
    linkopts = ["-lpthread"] + select({
        # `-lrt` (POSIX realtime) only exists on Linux.
        "@platforms//os:linux": ["-lrt"],
        "//conditions:default": [],
    }),
    autogen = True,
    configure_in_place = True,
    # On macOS rules_foreign_cc sets AR to Apple's `/usr/bin/libtool`, whose
    # CLI is incompatible with the `ar`-style `$(AR) crus <archive> <objs>`
    # that jemalloc's Makefile uses. Force a real `ar`.
    env = select({
        "@platforms//os:macos": {"AR": "/usr/bin/ar"},
        "//conditions:default": {},
    }),
    lib_source = ":all",
    out_shared_libs = select({
        "@platforms//os:macos": ["libjemalloc.dylib"],
        "//conditions:default": ["libjemalloc.so"],
    }),
    out_static_libs = ["libjemalloc.a"],
)
