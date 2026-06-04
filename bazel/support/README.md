# Bazel Support

## Bazel Install
- Make sure that Bazel is installed on your system.

### [Ubuntu/Debian](https://bazel.build/install/ubuntu)

### [Fedroa/Centos](https://bazel.build/install/redhat)

### Arch/Manjaro
```shell
sudo pacman -S bazel
```

## Compile and build with bazel on flare
```bash
git clone https://github.com/Tencent/flare.git
cd flare

bazel build ...                      # compile all target
bazel test ...                       # compile and execute tests
```

- See [this](https://bazel.build/run/build) get more infomation
- `...` Indicates recursively scan all targets, recognized as `../..` in `oh-my-zsh`, can be replaced by other `shell` or `bash -c 'commond'` to run, such as `bash -c 'bazel build' ...` or use `bazel build ...:all`

## Using {flare} as a dependency

The following minimal example shows how to use {flare} as a dependency within a Bazel project.  

The following file structure is assumed:
```shell
.
├── .bazelrc
├── BUILD.bazel
├── .gitignore
├── main.cc
├── REAMD.md
└── WORKSPACE.bazel
```
You can find these files in this directory.

*[main.cc](main.cc)*
```cpp
#include "flare/base/logging.h"

int main(int argc, char **argv) { FLARE_LOG_INFO("Hello flare!"); }
```

The expected output of this example is `Hello flare!`.

*[WORKSPACE.bazel](WORKSPACE.bazel)*  
You can also use the `http_archive` method to download flare:
```Python
# Use http_archive
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
FLARE_VERSION = ""
FLARE_SHA256 = ""

http_archive(
    name = "com_github_tencent_flare",
    sha256 = FLARE_SHA256,
    urls = ["https://github.com/Tencent/flare/archive/refs/tags/{version}.zip".format(version = FLARE_VERSION)],
    strip_prefix = "flare-{version}".format(version = FLARE_VERSION),
)

load("@com_github_tencent_flare//bazel:deps.bzl", "flare_dependencies")

flare_dependencies()

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()
```

*[BUILD.bazel](BUILD.bazel):*
```Python
cc_binary(
    name = "main",
    srcs = ["main.cc"],
    deps = [
        "@com_github_tencent_flare//flare/base:logging",
    ],
)
```

The BUILD file defines a binary named main that has a dependency to {flare}.

To execute the binary, you can run `bazel run //:main`.
