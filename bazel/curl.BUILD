load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "all",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

configure_make(
    name = "curl",
    configure_options = [
        "--with-nghttp2=$EXT_BUILD_DEPS/nghttp2",
        "--with-ssl=$EXT_BUILD_DEPS",
        "--with-zlib=$EXT_BUILD_DEPS/zlib",
        "--enable-shared",
        "--enable-static",
        "--with-pic",
        "--disable-ldap",
        "--without-brotli",
        "--without-libidn",
        "--without-libidn2",
        "--without-librtmp",
        "--without-libpsl",
    ],
    # macOS: use a real `ar`; rules_foreign_cc sets AR to Apple's libtool,
    # whose CLI is incompatible with the `ar`-style invocation autotools uses.
    env = select({
        "@platforms//os:macos": {"AR": "/usr/bin/ar"},
        "//conditions:default": {},
    }),
    lib_source = ":all",
    # out_shared_libs = ["libcurl.so"],
    out_static_libs = ["libcurl.a"],
    deps = [
        "@com_github_google_boringssl//:crypto",
        "@com_github_google_boringssl//:ssl",
        "@com_github_nghttp2_nghttp2//:nghttp2",
        "@zlib",
    ],
)
