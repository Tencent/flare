# Foreign Build: 在 Blade 中构建和使用其他构建工具构建的包 #

## 背景 ##

我们大量依赖一些优秀的第三方库，但是这些库绝大多数都是其他构建系统构建的，比如 GNU Autotools 或者 CMake。
我们过去的做法是将源代码包解压，分析其构建文件，为其编写 BUILD 文件。对于用了 Autoconf 的包，还要先用 configure
命令生成一些头文件，由于在不同的系统下生成的头文件还可能不一样（比如 x86_64 和 i386），可能还得手动修改生成的头文件。
导致无论是引入还是升级第三方库，都很麻烦。而大多数库我们不用修改其源代码。

因此我们引入了对外部构建系统的支持，允许在 Blade 中调用其他构建系统，并正确引用其构建的结果，需要做的就只是为这些库编写构建规则。
目前只支持类 Autotools 和 CMake 两种外部构建工具。

## 规则 ##

### autotools_build ###

用于构建用 [GNU Autotools](https://en.wikipedia.org/wiki/GNU_Autotools) 或者兼容的类似构建系统用法的包。

我们约定外部构建工具构建的代码都是以压缩包的形式存在在代码库中，经过解压、`configure`、`make`、`make install` 等过程，
安装到 `blade-bin`（也就是 `build64_release`）下的同路径的输出目录下。安装后的目录至少存在 `include` 和 `lib` 两个子目录。
比如对于 `thirdparty/openssl/openssl-1.0.u2.tar.gz`，其安装目录为 `build64_release/thirdparty/openssl/`

因此，Autotools 包的构建中存在两个顶级目录：

* 解压目录 源代码解压所在的目录，起始目录为 blade-bin 目录下的和源代码同布局的目录，不过通常的源代码包里都有自己的一层子目录。
* 安装目录 make install 目录，起始目录同解压目录，子目录由 `install_dir` 属性决定。

C/C++ 包的头文件常见的组织方式有两种，一种是不带路径，比如 `#include "zlib.h"`，另一种则是带上固定的前缀路径，
比如 `#include "openssl/md5.h"`，但是这两种方式通常都是位于在 `include` 子目录下。编译时需要把 `include` 子目录
加入到头文件搜索路径中，autotools_build 规则已经自动做了这件事。如果头文件的目录名不是 `include`，可以通过 `include_dir` 属性修改。

为了更符合 Blade 头文件的目录布局，对于 include 下的头文件，`*_build` 规则还会生成转发头文件。比如对于
`build64_release/thirdparty/zlib/include/zlib.h`，生成的转发头文件为 `build64_release/thirdparty/zlib/zlib.h`，
使用时 `#include "thirdparty/zlib/zlib.h"` 即可。

对于带目录前缀的包含路径，比如 `build64_relase/thirdparty/openssl/include/openssl/md5.h`，默认生成的转发头文件路径为
`build64_relase/thirdparty/openssl/openssl/md5.h`，这看起来很丑，可以通过设置 `strip_include_prefix='openssl'`，
去掉路径中的第二个 `openssl`，最终生成的路径为`build64_relase/thirdparty/openssl/md5.h`，包含时使用 `thirdparty/openssl/md5.h` 即可。

生成库的名字通过 lib_names 属性指定，库所在的目录为 install_dir 下的 `lib` 子目录，因此对于 `lib/libz.a`，需要指定 lib_names = ['z']，
如果生成库还有其他子目录层级，则可以通过把 lib_names 的成员指定为 tuple 来，比如 opencv 下的 `lib64/opencv4/3rdparty/libippicv.a`，需要用
`('opencv4/3rdparty', 'ippicv')` 来描述。

某些包，比如 openssl，并不是真正的用 Autotools 构建的项目，但是其构建过程 Config、make、make install 和安装后的目录布局类似于 Autotools 包，
因此也可以用本规则构建。

属性：

* name 目标名，推荐包名（不带版本）加 `_build` 后缀，比如 `openssl_build`。
* source_package 源代码压缩包的文件名，支持 `*.tar.[gbx]z`、`*t[gbx]z` 和 `*.zip`，比如 `openssl-1.0.2u.tar.gz`。
* package_name 包的名字，不含版本号，比如 `openssl`，`zlib`。
* source_dir 压缩包里解压后进行构建的目录名，也就是 `configure` 所在的子目录，通常情况是压缩包的文件名去掉压缩扩展名，比如 `openssl-1.0.2u`，
  这种情况下不需要显式设置，但是也可能不是，具体可以用解压工具查看。比如 icu4c 库，其 `configure` 文件就在 `icu/source` 子目录下。
* lib_names 包中所含的库的名字，比如 zlib 只有 `z` 一个库，而 openssl 却含有 `crypto` 和 `ssl` 两个库。
* configure_file_name `configure` 文件的文件名，默认为 `configure`，一般不需要显式设置，但是对于 openssl 这样的伪 Autotools 包，其对应的文件却叫 `Config`。
* configure_options 传给 `configure` 命令的选项，比如 `--enable-shared`，`--enable-pic` 等，
  详情参见[文档](https://www.gnu.org/prep/standards/html_node/Configuration.html)，对于具体的库包，可以解压后执 `configure --help` 查看。
* install_dir 安装到的目录名，默认为空。也支持一个目录下多个 foreign build 目标，此时需要用不同的 install_dir 区分开。
* include_dir 安装后头文件所在的子目录，默认为 'include', "openssl"。
* strip_include_prefix 生成转发头文件时希望去除的路径部分。
* deps 构建本包需要的其他依赖，通常是 `configure` 的 `--with-xxx` 所需要的库的构建规则。
* install_target `make install` 的目标名，通常是 `install` 因此不用显式设置，但是某些情况下可能是其他目标，
  比如 openssl 用 make install_sw 来只安装库而不安装文档，加快构建速度并[避免某些错误](https://trac.nginx.org/nginx/ticket/583)。

### cmake_build ###

CMake 的构建过程类似 Autotools，不过其对应于 configure 的阶段是生成 Makefile，我们称之为 `generate`。

CMake 鼓励 `out-of-source` 构建模式，也就是生成的文件不在源代码目录中，很多库也只支持这种构建模式，
因此我们也采用这种模式，因此和 Autotools 构建比起来，还会额外生成一个构建目录。

特有属性：

* cmake_options 类似 autotools_build 的 configure_options，具体参考 [CMake 文档](https://cmake.org/cmake/help/latest/manual/cmake.1.html)。
  CMAKE_INSTALL_PREFIX 的值由规则内部生成，请勿自行指定。
* lib_dir 库所在的子目录名，默认为 `lib`，但是某些包（比如 opencv）在 64 位构建下是在 `lib64` 目录下。

cmake_build 的绝大多数属性同 autotools_build，不再赘述。

### foreign_cc_library ###

参见 [Blade 文档](https://github.com/chen3feng/blade-build/blob/master/doc/zh_CN/build_rules/cc.md#foreign_cc_library)。

foreign_cc_library 的 deps 里要带上对构建包的目标的依赖，比如对于 openssl，crypto 需要依赖 openssl_build。
需要注意的是，对于包含多个库的包，这些库之间往往存在依赖关系，需要正确描述，比如对于 openssl，ssl 就依赖 crypto。

### autotools_cc_library ###

对于只有一个库的包（或者你只需要一个库）的情况，可以用 autotools_cc_library 来简化规则。

### cmake_cc_library ###

类似于 autotools_cc_library。

## 示例 ##

**nghttp2**

```python
include('//thirdparty/foreign_build.bld')  # 引入规则

autotools_build(
    name = 'nghttp2_build',
    source_package = 'nghttp2-1.41.0.tar.bz2',
    package_name = 'nghttp2',
    lib_names = ['nghttp2'],  # 库文件名为 lib/libnghttp2.a
    # 只生成库，不生成可执行文件，不生成动态库，开启 PIC
    configure_options = '--enable-lib-only --disable-shared --with-pic',
    # 生成转发头文件时，去掉路径中多余的 `nghttp2` 片段
    strip_include_prefix = 'nghttp2',
)

foreign_cc_library(
    name = 'nghttp2',
    install_dir = '',
    deps = [
      # 需要先构建出来
      ':nghttp2_build',
    ],
)
```

因为只有一个库，显然可以用 autotools_cc_library 来简化代码：

```python
autotools_cc_library(
    name = 'nghttp2',
    source_package = 'nghttp2-1.41.0.tar.bz2',
    package_name = 'nghttp2',
    configure_options = '--enable-lib-only --disable-shared --with-pic',
    install_dir = '',
    strip_include_prefix = 'nghttp2',
)
```

**openssl**

openssl 包含 crypto 和 ssl 两个库，属于比较复杂的情况：

```python
include('//thirdparty/foreign_build.bld')

autotools_build(
    name = 'openssl_build',
    source_package = 'openssl-1.0.2u.tar.gz',
    package_name = 'openssl',
    lib_names = ['crypto', 'ssl'],
    configure_options = '-fPIC',
    configure_file_name = 'config',
    strip_include_prefix = 'openssl',
    install_target = 'install_sw',
)

foreign_cc_library(
    name = 'crypto',
    install_dir = '',
    deps = [
        # 对系统库的依赖不能少，因为里面用到了
        '#pthread',
        '#dl',
        # 需要依赖构建目标
        ':openssl_build',
    ],
)

foreign_cc_library(
    name = 'ssl',
    install_dir = '',
    deps = [
        # :ssl 库依赖 :crypto
        ':crypto',
        # 因为已经依赖了 :crypto，对构建目标的依赖可以省略
        ':openssl_build',
    ],
)
```

除了上述例子外，目前代码库中下列包采用 foreign_build 的方式构建，可供参考：

* curl
* ffmpeg
* nghttp2
* opencv 采用 CMake 构建
* openssl

## 调试 ##

由于第三方库的代码组织五花八门，即使有了这些辅助，通常也很难一下子就写出正确的构建规则。
建议编写规则之前先手工构建安装一遍，先解压到某个目录下，执行 ./configure --help 查看有哪些选项，
然后预定一个安装目录，执行 ./configure --prefix=安装目录的绝对路径，make，make install，进入安装
目录查看生成的目录结构，比如查看 `lib` 下有哪些 `*.a` 文件，`include` 下有没有子目录，等等。

## 常见问题 ##

### 解压后的目录名不同于压缩包名 ###

比如压缩包名为 `xxxlib-1.21.tar.gz`，但是解压后的目录名却是 `xxxlib-master`。

解决：指定 `source_dir` 参数：`source_dir = "xxxlib-master"`

### 不支持 out-of-source build ###

有些库不允许在解压后的源代码目录里直接构建，而是需要单独建一个空的 build 目录，避免构建结果污染源代码目录。
cmake 目标已经都是 out-of-source build 的了，用 autotools 的库，强制要求 out-of-source build 的较少，如果有的再考虑支持。

### 构建文件不在根目录下 ###

构建脚本文件（比如 `configure` 或者 `CMakeList.txt`）通常都在解压后的根目录下，有些库的构建脚本却不是，比如 `zstd` 库的 cmake 文件就在其 `build/cmake` 目录下

解决：指定 `source_dir`，带上额外的子目录名，比如 `source_dir = "cmake-1.4.5/build/cmake"`

### 库目录找不到 ###

有些构建系统 install 的安装目录可能会是 `lib64` 而不是常规的 `lib` 子目录。

解决：在 `autotools_build` 及其相关的 `foreign_cc_library` 中都指定相同的 `lib_dir` 参数，例如 `lib_dir = 'lib64'`。

## 原理 ##

除了 `foreign_cc_library` 外，本文档描述的其他规则都是基于 `gen_rule` 而实现的“宏”。构建时，会经历解压、
`configure`（对 CMake 是 generate），`make`，`make install` 过程，安装到 build 目录下对应的子目录下，
然后再用 `foreign_cc_library` 来描述其中的库。
