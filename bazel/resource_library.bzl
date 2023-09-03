def resource_library(
        name,
        srcs,
        visibility = None,
        **kargs):
    gen_src = name + ".c"
    gen_hdr = name + ".h"
    gen_srcs = [s + ".c" for s in srcs]

    native.genrule(
        name = "resource_gen_" + name,
        srcs = srcs,
        outs = [gen_hdr, gen_src] + gen_srcs,
        cmd = """
            $(location //bazel:generate_resource_lib) $(OUTS) $(SRCS)
            """,
        tools = [
            "//bazel:generate_resource_lib",
        ],
    )

    native.cc_library(
        name = name,
        srcs = [gen_src] + gen_srcs,
        hdrs = [gen_hdr],
        visibility = visibility,
        **kargs
    )
