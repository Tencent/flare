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
        "--enable-static",
        "--with-pic",
    ] + select({
        # macOS: don't build a .dylib. Its install_name points into the sandbox
        # build dir, which breaks curl's configure run-time link check (curl
        # only consumes the static lib anyway).
        "@platforms//os:macos": ["--disable-shared"],
        "//conditions:default": ["--enable-shared"],
    }),
    # macOS: use a real `ar`; rules_foreign_cc sets AR to Apple's libtool,
    # whose CLI is incompatible with the `ar`-style invocation autotools uses.
    env = select({
        "@platforms//os:macos": {"AR": "/usr/bin/ar"},
        "//conditions:default": {},
    }),
    lib_source = ":all",
    # out_shared_libs = ["libnghttp2.so"],
    out_static_libs = ["libnghttp2.a"],
)
