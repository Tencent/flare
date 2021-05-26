cc_library(
    name = 'protobuf',
    deps = '//thirdparty/protobuf-3.4.1:protobuf'
)

cc_library(
    name = 'protoc',
    deps = '//thirdparty/protobuf-3.4.1:protoc_lib'
)

proto_library(
    name = 'proto_test_proto',
    srcs = 'proto_test.proto',
)

cc_test(
    name = 'proto_test',
    srcs = 'proto_test.cc',
    deps = ':proto_test_proto',
)
