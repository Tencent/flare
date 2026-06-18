# thirdparty 依赖与 vcpkg 迁移状态

本目录的第三方库正在从「本地 foreign build（源码 tarball + autotools/cmake）」逐步迁移到
**vcpkg**（通过 blade 的 `vcpkg#<port>:<lib>` 机制，详见 `BLADE_ROOT` 的 `vcpkg_config`）。

迁移后 `//thirdparty/<lib>:<target>` 的引用方式保持不变 —— `BUILD` 改成一个薄
`cc_library` 包装，`deps=['vcpkg#<port>:<lib>']`，业务代码无需改动。

## ✅ 已迁移到 vcpkg

| 库 | 版本 | vcpkg_config 关键项 | 说明 |
|---|---|---|---|
| fmt | 7.1.3（钉定） | `'fmt': '7.1.3'` | 无特殊处理 |
| gflags | 2.2.2（钉定） | `link_all_symbols=True` | 单例（全局 flag 注册表）。`link_all_symbols` whole-archive 静态 `.a`，保证静态链接的 protoc 插件里 flag 注册的静态初始化即使未被引用也运行 |
| glog | 0.7.1（钉定） | `link_all_symbols=True`, `cmake_options=['-DGFLAGS_NOTHREADS=OFF']` | 单例（在 gflags 里注册 logtostderr 等）。`GFLAGS_NOTHREADS=OFF` 绕过 gflags-config.cmake 在**静态** gflags 下的模板 bug（默认会找不存在的 `gflags_nothreads_static`） |
| zlib | baseline | `include_prefix='zlib'` | flare 用 `zlib/zlib.h`，vcpkg 把头放在 include 顶层 |
| lz4 | baseline | `include_prefix='lz4'` | flare 用 `lz4/<h>`，vcpkg 在 include 顶层 |
| zstd | baseline | `include_prefix='zstd'` | 同上 |
| snappy | baseline | `include_prefix='snappy'`, `cmake_options=['-DSNAPPY_WITH_RTTI=ON']` | vcpkg 的 snappy 默认 `-fno-rtti`，而 flare 用到 `snappy::Sink/Source` 的 RTTI |
| xxhash | baseline | `include_prefix='xxhash'` | 迁移时删掉了源码树里残留的整份 upstream（其 `xxhash.h` 经 `thirdparty/` extra_inc 把 vcpkg 的头 shadow 掉了，导致一直在用旧的 in-tree 头） |
| protobuf | 3.21.12（钉定） | `'protobuf': {'version': '3.21.12'}` | 最后一个不依赖 Abseil 的版本（v22+ 起 Abseil 成为硬依赖）。`proto_library_config` 用 vcpkg 的 protoc + libprotobuf（`vcpkg#protobuf`，生成代码匹配运行时）。默认 `'auto'` linkage 让所有 proto dylib 共享同一份 `libprotobuf`，即同一个 `DescriptorPool::generated_pool()`——否则静态 protobuf 被 `proto_library`（`link_all_symbols=True`）whole-archive 进每个 proto dylib，各有一份 pool，跨 dylib 的 `FindFileByName` 取不到 → 段错误。需改 flare 源码适配 3.4→3.21 API（删掉与 `rpc_options.cc` 重复的 ext-10003 注册——3.21 重复注册会 fatal；`Status::error_message()`→`ToString()`）；`cc_flare_library` 把 `vcpkg#` protoc 解析到 blade 安装路径。详见 #184 |
| yaml-cpp | baseline（0.9.0） | `link_all_symbols=True` | 被 `flare/base/monitoring:init`（构建成 `.dylib`）依赖。`link_all_symbols` 把静态 `.a` 整体 whole-archive，确保消费者拿到全部 yaml-cpp 符号 |
| benchmark | 1.7.1（钉定） | `'benchmark': {'version': '1.7.1', 'linkage': 'static'}` | 钉到 1.7.1：1.8.0 起把 `benchmark::DoNotOptimize(const T&)` 标记 deprecated，而 flare 的 `*_benchmark.cc` 大量用它，gcc + `-Werror=deprecated-declarations` 会编译失败（clang/macOS 不触发）；1.7.1 是该弃用之前的最后一系列。`linkage='static'`：google/benchmark 无共享构建也不需要（每个 `*_benchmark` 可执行文件各链一次），默认 `'auto'` 会因构建不出 `.dylib` 而失败 |
| gtest | 1.17.0（钉定） | `'gtest': {'version': '1.17.0'}` | vcpkg port 名为 `gtest`；通过 `cc_test_config.gtest_libs / gtest_main_libs` 接到 `//thirdparty/googletest:{gtest,gtest_main,gmock,gmock_main}`。`*_main` 库装在 vcpkg 的 `lib/manual-link/`（blade 已能在该子目录解析）。单例（UnitTest 注册表）由默认 `'auto'` 自动给到单份共享实例 |
| openssl | baseline（3.x） | `'openssl': {}` | 升级 1.1.1w→3.x（vcpkg 无 1.1.1）。`flare/io/util/openssl.cc` 删掉过时的 1.0.x 初始化/线程锁/ENGINE 仪式（1.1+ 自动初始化且内部线程安全）；`base/crypto/{md5,sha}` 加 `-Wno-deprecated-declarations`（低层 MD5/SHA/HMAC 在 3.x 被弃用但 4.0 前仍可用，未改写成 EVP）。原生 `include/openssl/` 布局。详见 #190 |
| nghttp2 | baseline | `'nghttp2': {}` | API 稳定的版本提升；仅被 curl 使用。原生 `include/nghttp2/` 布局 |
| curl | baseline（8.x） | `'curl': {'features': ['openssl', 'http2']}` | API 稳定提升（7→8）。openssl TLS + nghttp2 http2（zlib 自动探测）。macOS 需要的系统 framework（SystemConfiguration/Security/CoreFoundation/CoreServices）由 blade 从 curl 的 pkg-config 自动解析 `-framework`（blade-build #1337）。wrapper 显式依赖同一 vcpkg 装的 openssl/nghttp2/zlib。详见 #190 |

**默认 `linkage='auto'`**（blade 的默认值，对齐 `cc_library`）：静态 `.a` 始终构建；
动态库**按需**构建——仅当某个 `dynamic_link` 二进制真正依赖时（analyze 阶段设置的
`generate_dynamic`，vcpkg 安装在 analyze 之后跑，故只为被需要的 port 建共享库，落在独立的
`blade-<triplet>-shared` 树）。这样静态链接的构建期工具（如 protoc 插件）拿到自包含的 `.a`，
而 `dynamic_link` 二进制共享同一份动态库——进程级单例（protobuf descriptor pool、
gflags/glog/gtest 注册表）因此自动只有一份，无需逐库标注。详见 blade 文档
`doc/*/build_rules/vcpkg.md`。仅非默认选择（`benchmark` 的 `'static'`、各 `link_all_symbols`
/ `include_prefix` / `cmake_options`）才在 `BLADE_ROOT` 显式写出。

**配套的 blade 改动**：默认 `linkage='auto'`；为链接 vcpkg 动态库的 `cc_binary` 烤
`LC_RPATH`，使 `@rpath/<lib>` 在原地运行、`blade run`、构建期当工具执行（protoc 插件）时都能
加载，无需 `DYLD_LIBRARY_PATH`；chainload 工具链设 `CMAKE_POSITION_INDEPENDENT_CODE`，让静态
vcpkg `.a` 能链进 `.so`。

**配套的 flare 改动**：protoc 插件（`v1_plugin` / `v2_plugin`）设 `dynamic_link=False` ——
它们是构建期工具，静态/自包含链接，避免运行期解析 `@rpath` 动态库。

## ❌ 保留 foreign build（无法 / 不宜迁移）

### 硬性阻塞——无法迁移

- **ctemplate**（本地 2.4）
  vcpkg 的 ctemplate port **只支持 Windows**（`"supports": "windows & !arm"`）。它的
  CMakeLists 强制编译 `src/windows/port.cc`，该文件首行即
  `#error "You should only be including windows/port.cc in a windows environment!"`，
  并依赖 Win32 API（`GetTempPathA`、`HANDLE`、`_mkdir`、`INVALID_HANDLE_VALUE`）及
  `base/mutex.h`（非 Windows 架构无实现）。在 macOS/Linux 上即便加 `--allow-unsupported`
  也会编译失败。要走 vcpkg 只能自写 overlay port，成本/维护代价过高。

  > protobuf 原先也列在这里（钉死 3.4.1），现已升级到 vcpkg 3.21.12 并迁移完成，
  > 见上方 ✅ 表格与 #184。

- **opentracing-cpp**（本地 1.5.1）
  vcpkg 的 opentracing（baseline 1.6.0）内置的 `variant.hpp` 用了 `std::result_of`，它在
  C++17 起被弃用、C++20 移除，flare 的现代工具链（Xcode clang）直接编译失败（`no type named
  'result_of' in namespace 'std'`）。flare 的 foreign build 正是靠本地 `variant.patch` /
  `cxx17.patch` 修掉这点（bazel 侧也有对应的 `opentracing-cpp-cxx20.patch`）。vcpkg port 不带
  这些补丁，要走 vcpkg 只能自写带 patch 的 overlay port，成本过高。

### 版本钉死——vcpkg 只有差异过大的新版

flare 的 foreign build 钉死在特定旧版本（且常带 flare 本地 patch）；vcpkg baseline 只提供
高得多的版本，非 drop-in，迁移需要改代码 / 处理 ABI，且会丢失本地 patch。

| 库 | 本地版本 | vcpkg baseline | 阻塞点 |
|---|---|---|---|
| jsoncpp | 0.10.7 | 1.9.6 | 0.x→1.x 大版本 API 变化（`Reader`/`Writer` 弃用、头布局 `jsoncpp/` vs `json/`）；本地有 `no_multi_arch_libdir.patch` |

> 并非「技术上不可能」，而是需要改适配代码。curl 簇里的 openssl/nghttp2/curl 已按此
> 路子升级完成（见上方 ✅ 表格与 #190），jsoncpp 是该簇仅剩的一个，跟踪于
> [Tencent/flare#179](https://github.com/Tencent/flare/issues/179)。

### 刻意保留 foreign

- **gperftools（2.8）/ jemalloc（5.2.1）**
  分配器（malloc 替换）。flare 对它们有专门接线：`cc_config.gperftools_libs`、
  `pprof_path='thirdparty/gperftools/bin/pprof'`（指向源码树里的 pprof 工具）、特定的
  链接顺序 / whole-archive。vcpkg 虽有这两个 port，但替换 malloc 这类全局行为 + 自带
  pprof 工具是 flare 专属，迁移高风险、低收益。

- **blake3 / rapidxml**
  都是源码树里直接 vendored 的头/源文件（blake3 是一组 `.c`，rapidxml 头文件-only），并非
  foreign build——没有 tarball/cmake_build 样板可省。迁到 vcpkg 反而要引入 vcpkg 依赖 +
  bazel 侧 alias（参考 xxhash 的做法），收益很小，暂不动。

## 迁移一个库的步骤（备忘）

1. 在 `BLADE_ROOT` 的 `vcpkg_config.packages` 加一条；需要时配 `include_prefix`
   （flare 的 include 路径 ≠ vcpkg 的 include 布局时）、`linkage`、`cmake_options`。
2. 把 `thirdparty/<lib>/BUILD` 改成薄 `cc_library(deps=['vcpkg#<port>:<lib>'])` 包装，
   保持原 target 名不变。
3. 删掉源码树里残留的 in-tree 头文件（否则 `cc_config.extra_incs` 里的 `thirdparty/`
   会让它 shadow vcpkg 的头——参见 xxhash 的教训）。
4. 构建一个消费者验证；跑 `blade test` 确认运行期能加载到 `@rpath` 动态库。
