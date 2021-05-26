# README

## File: protoc-2.5.0

修改 `protoc-2.5.0`，对 proto 文件中 `cc_enable_arenas` 选项不报未知 Option 错误。

### 为什么要改？
代码库中多数 proto 文件需要生成 `Java` 和 `C++` 代码，生成 `Java` 代码使用 `protoc-2.5.0`，生成 `C++` 代码使用 `protoc-3.4.1`。测试发现在一些负载下，`C++` 服务利用 `cc_enable_arenas` 选项可以提升性能。但是 `protoc-2.5.0` 不识别这个选项，造成 `C++` 代码无法开启 arenas 特性。考虑到开启这个选项不影响 `Java` 代码生成，所以我们基于 `protobuf-2.5.0` 源码修改了 protoc，使得它在生成 `Java` 代码时对选项 `cc_enable_arenas` 不报错。

### 构建步骤

1. 从 Protobuf 网站下载 [2.5.0 版本](https://github.com/protocolbuffers/protobuf/releases/download/v2.5.0/protobuf-2.5.0.tar.bz2)的源码，解压文件。

2. 进入解压目录，编译生成 `protoc`

```
./configure --disable-shared && make -j8
```

3. 修改 descriptor.proto，在 FileOptions 中增加定义:

```
--- protobuf-2.5.0-orig/src/google/protobuf/descriptor.proto	2013-02-27 01:56:42.000000000 +0800
+++ protobuf-2.5.0-arena/src/google/protobuf/descriptor.proto	2020-04-28 17:20:24.000000000 +0800
@@ -297,6 +297,10 @@
   optional bool java_generic_services = 17 [default=false];
   optional bool py_generic_services = 18 [default=false];

+  // Enables the use of arenas for the proto messages in this file. This applies
+  // only to generated classes for C++.
+  optional bool cc_enable_arenas = 31 [default=false];
+
   // The parser stores options it doesn't recognize here. See above.
   repeated UninterpretedOption uninterpreted_option = 999;
```

4. 重新生成 `protoc`，替换 `thirdparty/protobuf/bin/protoc-2.5.0` 文件

```
./generate_descriptor_proto.sh && make -j16
```

### 备注

1. 编译生成的 protoc 需要兼容 tlinux1 环境，推荐静态链接。

2. 需要更新 `thirdparty/google/protobuf/descriptor.proto`。

  protoc 是从 DescriptorPool 中动态找 FileOptions 定义，以此判断是否有未知选项。代码库中一些 proto 定义 import 了 `thirdparty/google/protobuf/descriptor.proto` 文件，影响了 DescriptorPool，所以需要修改这个文件。这个修改在 FileOptions 增加一个字段，不影响 Java 代码生成。
