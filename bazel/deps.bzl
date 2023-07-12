"""Load dependencies needed to use the flare library as a 3rd-party consumer."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def flare_dependencies():
    """Loads common dependencies needed to use the flare library."""
    if not native.existing_rule("com_github_fmtlib_fmt"):
        http_archive(
            name = "com_github_fmtlib_fmt",
            build_file = Label("//bazel:fmt.BUILD"),
            urls = [
                "https://github.com/fmtlib/fmt/archive/refs/tags/7.1.3.zip",
            ],
            strip_prefix = "fmt-7.1.3",
            sha256 = "50f2fd9f697f89726ae3c7efe84ae48c9e03158a2958eea496eeaa0fb190adb6",
        )

    if not native.existing_rule("com_github_google_glog"):
        http_archive(
            name = "com_github_google_glog",
            urls = [
                "https://github.com/google/glog/archive/refs/tags/v0.4.0.zip",
            ],
            strip_prefix = "glog-0.4.0",
            sha256 = "9e1b54eb2782f53cd8af107ecf08d2ab64b8d0dc2b7f5594472f3bd63ca85cdc",
        )

    if not native.existing_rule("com_github_gflags_gflags"):
        http_archive(
            name = "com_github_gflags_gflags",
            urls = ["https://github.com/gflags/gflags/archive/v2.2.2.tar.gz"],
            sha256 = "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf",
            strip_prefix = "gflags-2.2.2",
        )

    if not native.existing_rule("com_github_jsoncpp"):
        http_archive(
            name = "com_github_jsoncpp",
            urls = ["https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/0.10.7.zip"],
            sha256 = "165b2ac2f17601b500bf01b0b9a3b304f620986e0da5d988397d64f8f3c942c3",
            strip_prefix = "jsoncpp-0.10.7",
            build_file = Label("//bazel:jsoncpp.BUILD"),
        )

    if not native.existing_rule("com_google_protobuf"):
        http_archive(
            name = "com_google_protobuf",
            strip_prefix = "protobuf-3.15.7",
            urls = ["https://github.com/protocolbuffers/protobuf/releases/download/v3.15.7/protobuf-cpp-3.15.7.zip"],
            sha256 = "c0332cd6f88ad2401cec8797d2ac6c8b27d357c999e025679a870f6cb24fbf17",
        )

    # protobuf use zlib, so name is not com_github_madler_zlib
    if not native.existing_rule("zlib"):
        http_archive(
            name = "zlib",
            build_file = Label("//bazel:zlib.BUILD"),
            sha256 = "d14c38e313afc35a9a8760dadf26042f51ea0f5d154b0630a31da0540107fb98",
            strip_prefix = "zlib-1.2.13",
            urls = [
                "https://github.com/madler/zlib/releases/download/v1.2.13/zlib-1.2.13.tar.xz",
                "https://zlib.net/zlib-1.2.13.tar.xz",
            ],
        )

    if not native.existing_rule("com_github_jbeder_yaml_cpp"):
        http_archive(
            name = "com_github_jbeder_yaml_cpp",
            build_file = Label("//bazel:yaml-cpp.BUILD"),
            urls = ["https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.6.3.zip"],
            strip_prefix = "yaml-cpp-yaml-cpp-0.6.3",
            sha256 = "7c0ddc08a99655508ae110ba48726c67e4a10b290c214aed866ce4bbcbe3e84c",
        )

    if not native.existing_rule("com_github_facebook_zstd"):
        http_archive(
            name = "com_github_facebook_zstd",
            build_file = Label("//bazel:zstd.BUILD"),
            urls = ["https://github.com/facebook/zstd/archive/refs/tags/v1.4.5.zip"],
            strip_prefix = "zstd-1.4.5",
            sha256 = "b6c537b53356a3af3ca3e621457751fa9a6ba96daf3aebb3526ae0f610863532",
        )

    if not native.existing_rule("com_github_google_snappy"):
        http_archive(
            name = "com_github_google_snappy",
            build_file = Label("//bazel:snappy.BUILD"),
            urls = ["https://github.com/google/snappy/archive/refs/tags/1.1.8.zip"],
            strip_prefix = "snappy-1.1.8",
            sha256 = "38b4aabf88eb480131ed45bfb89c19ca3e2a62daeb081bdf001cfb17ec4cd303",
        )

    if not native.existing_rule("com_github_lz4_lz4"):
        http_archive(
            name = "com_github_lz4_lz4",
            build_file = Label("//bazel:lz4.BUILD"),
            urls = ["https://github.com/lz4/lz4/archive/refs/tags/v1.9.3.zip"],
            strip_prefix = "lz4-1.9.3",
            sha256 = "4ec935d99aa4950eadfefbd49c9fad863185ac24c32001162c44a683ef61b580",
        )

    if not native.existing_rule("com_github_google_boringssl"):
        http_archive(
            name = "com_github_google_boringssl",
            sha256 = "5d299325d1db8b2f2db3d927c7bc1f9fcbd05a3f9b5c8239fa527c09bf97f995",  # Last updated 2022-10-19
            strip_prefix = "boringssl-0acfcff4be10514aacb98eb8ab27bb60136d131b",
            urls = ["https://github.com/google/boringssl/archive/0acfcff4be10514aacb98eb8ab27bb60136d131b.tar.gz"],
        )

    if not native.existing_rule("com_github_openssl_openssl"):
        http_archive(
            name = "com_github_openssl_openssl",
            build_file = Label("//bazel:openssl.BUILD"),
            sha256 = "ecd0c6ffb493dd06707d38b14bb4d8c2288bb7033735606569d8f90f89669d16",
            strip_prefix = "openssl-1.0.2u",
            urls = ["https://www.openssl.org/source/old/1.0.2/openssl-1.0.2u.tar.gz"],
        )

    if not native.existing_rule("com_github_nghttp2_nghttp2"):
        http_archive(
            name = "com_github_nghttp2_nghttp2",
            build_file = Label("//bazel:nghttp2.BUILD"),
            urls = ["https://github.com/nghttp2/nghttp2/releases/download/v1.41.0/nghttp2-1.41.0.tar.bz2"],
            strip_prefix = "nghttp2-1.41.0",
            sha256 = "645ca078e7ec276dcfa27175f3af6140c8badc7358ec9d2892b6ab2bcee72240",
        )

    if not native.existing_rule("com_github_curl_curl"):
        http_archive(
            name = "com_github_curl_curl",
            build_file = Label("//bazel:curl.BUILD"),
            urls = [
                "https://mirror.bazel.build/curl.se/download/curl-7.71.0.tar.gz",
                "https://curl.se/download/curl-7.71.0.tar.gz",
                "https://github.com/curl/curl/releases/download/curl-7_74_0/curl-7.71.0.tar.gz",
            ],
            strip_prefix = "curl-7.71.0",
            sha256 = "62b2b1acee40c4de5a4913e27a4b4194813cf2b7815b73febec7ae53054646ca",
        )

    if not native.existing_rule("com_github_opentracing_opentracing_cpp"):
        http_archive(
            name = "com_github_opentracing_opentracing_cpp",
            build_file = Label("//bazel:opentracing-cpp.BUILD"),
            urls = ["https://github.com/opentracing/opentracing-cpp/archive/refs/tags/v1.5.1.zip"],
            strip_prefix = "opentracing-cpp-1.5.1",
            sha256 = "7a007a4cd987fb86ce05eeec8fe9e9f2052988c835bb6c738ec00bc167750f81",
        )

    if not native.existing_rule("com_github_olafvdspek_ctemplate"):
        http_archive(
            name = "com_github_olafvdspek_ctemplate",
            build_file = Label("//bazel:ctemplate.BUILD"),
            urls = ["https://github.com/OlafvdSpek/ctemplate/archive/5761f943039451081f6cfa1b95527ef9ac9f2c11.zip"],
            strip_prefix = "ctemplate-5761f943039451081f6cfa1b95527ef9ac9f2c11",
            sha256 = "d3fef34d56abc3e945358b4e6707123975cc67759040e4bf14f7a8aea4df87ff",
        )
    
    if not native.existing_rule("com_github_gperftools_gperftools"):
        http_archive(
            name = "com_github_gperftools_gperftools",
            build_file = Label("//bazel:gperftools.BUILD"),
            urls = ["https://github.com/gperftools/gperftools/releases/download/gperftools-2.8/gperftools-2.8.tar.gz"],
            strip_prefix = "gperftools-2.8",
            sha256 = "240deacdd628b6459671b83eb0c4db8e97baadf659f25b92e9a078d536bd513e",
        )
    
    if not native.existing_rule("com_github_jemalloc_jemalloc"):
        http_archive(
            name = "com_github_jemalloc_jemalloc",
            build_file = Label("//bazel:jemalloc.BUILD"),
            urls = ["https://github.com/jemalloc/jemalloc/archive/refs/tags/5.2.1.zip"],
            strip_prefix = "jemalloc-5.2.1",
            sha256 = "461eee78a32a51b639ef82ca192b98c64a6a4d7f4be0642f3fc5a23992138fd5",
        )

    if not native.existing_rule("rules_python"):
        http_archive(
            name = "rules_python",
            sha256 = "e5470e92a18aa51830db99a4d9c492cc613761d5bdb7131c04bd92b9834380f6",
            strip_prefix = "rules_python-4b84ad270387a7c439ebdccfd530e2339601ef27",
            urls = [
                "https://mirror.bazel.build/github.com/bazelbuild/rules_python/archive/4b84ad270387a7c439ebdccfd530e2339601ef27.tar.gz",
                "https://github.com/bazelbuild/rules_python/archive/4b84ad270387a7c439ebdccfd530e2339601ef27.tar.gz",
            ],
        )

    if not native.existing_rule("platforms"):
        http_archive(
            name = "platforms",
            urls = [
                "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.6/platforms-0.0.6.tar.gz",
                "https://github.com/bazelbuild/platforms/releases/download/0.0.6/platforms-0.0.6.tar.gz",
            ],
            sha256 = "5308fc1d8865406a49427ba24a9ab53087f17f5266a7aabbfc28823f3916e1ca",
        )

    if not native.existing_rule("bazel_skylib"):
        http_archive(
            name = "bazel_skylib",
            sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
            urls = [
                "https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
                "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
            ],
        )

    if not native.existing_rule("rules_foreign_cc"):
        http_archive(
            name = "rules_foreign_cc",
            sha256 = "61b74a99496470a27989b396b8331d93aba6c6cf21997533d6df3848eb5a095c",
            strip_prefix = "rules_foreign_cc-26c77008307c80a90fabc8fe3f7a72b961120a84",
            urls = ["https://github.com/bazelbuild/rules_foreign_cc/archive/26c77008307c80a90fabc8fe3f7a72b961120a84.tar.gz"],
        )
