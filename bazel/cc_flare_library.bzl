# Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
#
# Licensed under the BSD 3-Clause License (the "License"); you may not use this
# file except in compliance with the License. You may obtain a copy of the
# License at
#
# https://opensource.org/licenses/BSD-3-Clause
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

load("@com_google_protobuf//:protobuf.bzl", "proto_gen")

def _CCSrcs(srcs):
    ret = [s[:-len(".proto")] + ".flare.pb.cc" for s in srcs]
    return ret

def _CcHdrs(srcs):
    ret = [s[:-len(".proto")] + ".flare.pb.h" for s in srcs]
    return ret


def cc_flare_library(
        name,
        srcs,
        deps = [],
        visibility = None,
        **kargs):
    gen_srcs = _CCSrcs(srcs)
    gen_hdrs = _CcHdrs(srcs)

    native.genrule(
        name = "flare_gen_" + name,
        srcs = srcs,
        outs = gen_srcs + gen_hdrs,
        cmd = "$(location @com_google_protobuf//:protoc) " +
              "--proto_path=. " + 
              "-I=external/com_google_protobuf/src -I=. " +
              "--plugin=protoc-gen-flare_rpc=$(location //flare/rpc/protocol/protobuf/plugin:v2_plugin) " +
              "--flare_rpc_out=$(GENDIR) " +
              "$(SRCS)",
        tools = [
            "@com_google_protobuf//:protoc",
            "//flare/rpc/protocol/protobuf/plugin:v2_plugin",
            "//flare/rpc:rpc_options.proto",
            "@com_google_protobuf//:well_known_protos",
        ]   
    )

    native.cc_library(
        name = name,
        srcs = gen_srcs,
        hdrs = gen_hdrs,
        deps = [":flare_gen_" + name, "//flare/rpc:protobuf"] + deps,
        visibility = visibility,
        **kargs
    )
