# GFlags

我们通过[GFlags](https://github.com/gflags/gflags)控制框架的部分行为。

通常而言，GFlags对于我们的需求满足的很好。但是根据我们的实际场景，我们做了一些修改/扩充。

## 覆盖Flag的默认值

我们提供了[`FLARE_(FORCE_)OVERRIDE_FLAG`](../init/override_flag.h)以便于在代码中覆盖（不由自己定义的）Flag的默认值。

这些由`FLARE_(FORCE_)OVERRIDE_FLAG`指定的新的“默认值”会在`flare::Start`中应用。其应用时机位于解析GFlags之后，初始化Flare的其他部分之前。因此也可以用这一方式来覆盖Flare的一些配置参数。

如可以通过如下参数令程序默认将日志输出到`stderr`，并关闭glog的内部缓存：

```cpp
#include "flare/init/override_flag.h"

FLARE_OVERRIDE_FLAG(logbufsecs, 0);
FLARE_OVERRIDE_FLAG(logtostderr, true);

int main(int argc, char** argv) {
  flare::Start(...);
}
```

这两个宏有如下区别：

- `FLARE_OVERRIDE_FLAG`会首先检测被覆盖的Flag用户执行时有没有手动指定，如果有的话，则不会进行覆盖，以用户指定为准。
- `FLARE_FORCE_OVERRIDE_FLAG`始终会覆盖对应的Flag。这种情况下相关的Flag实际上就不再是“选项”了，而成为了一种“硬编码”的效果。
