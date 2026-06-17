# Thin wrappers over vcpkg protobuf 3.21.12 (see BLADE_ROOT vcpkg_config).
# Consumers keep depending on //thirdparty/protobuf:{protobuf,protoc}.
cc_library(
    name = 'protobuf',
    deps = ['vcpkg#protobuf:protobuf'],
)

cc_library(
    name = 'protoc',
    deps = ['vcpkg#protobuf:protoc'],
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
