load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "all",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

targets = [
    "profiler",
    "tcmalloc",
    "tcmalloc_and_profiler",
    "tcmalloc_debug",
    "tcmalloc_minimal",
    "tcmalloc_minimal_debug",
]

# @sa: https://github.com/bazelbuild/rules_foreign_cc/issues/768

[
    configure_make(
        name = target,
        configure_options = [
            "--enable-shared",
            "--enable-static",
            "--with-pic",
            "--enable-heap-checker",
            "--enable-heap-profiler",
            "--enable-cpu-profiler",
            "--enable-debugalloc",
            "--enable-frame-pointers",
            "--disable-libunwind",
        ],
        # macOS: use a real `ar`; rules_foreign_cc sets AR to Apple's libtool,
        # whose CLI is incompatible with the `ar`-style invocation autotools uses.
        env = select({
            "@platforms//os:macos": {"AR": "/usr/bin/ar"},
            "//conditions:default": {},
        }),
        lib_source = ":all",
        # out_shared_libs = [
        #     "lib" + target + ".so",
        # ],
        out_static_libs = [
            "lib" + target + ".a",
        ],
    )
    for target in targets
]
