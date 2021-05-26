# 读取配置

Flare提供了类似于GFlags的[从外界读取配置](../base/option.h)的能力，但是配置来源不限于`.flags`文件，也可以是网络上的配置中心等。

我们通过[`OptionPassiveProvider`](../base/option/option_provider.h)来支持不同的配置后端。

对于用户，我们提供了`Option<T>`模板类用于访问配置信息，其构造函数接受配置项的字符串标识及配置后端的字符串标识。但是通常而言，除非用户自行实现了对新的配置后端的支持，否则我们推荐使用已有的子类，如`GFlagsOption<T>`等（见后文）。

`Option<T>`类型定义简单描述如下，具体接口描述见注释。

```cpp
template <class T, class Parser>
class Option {
  // Not actually defined (publicly), for exposition only.
  //
  // `value_or_ref` is defined as `T` for primitive types, otherwise `const T&`.
  using value_or_ref = ...;

 public:
  // For subclasses (`XxxOption`), `provider` is implied and is not
  // configurable, only `name` (and optionally `default_value`) should be
  // provided.
  //
  // Usually you provide a string for `name`. For advanced uses, see below.
  //
  // MULTIPLE DEFINITION OF SAME OPTION LEADS TO CRASH. Use `extern Option<T>`
  // to declare instead.

  // Fixed options are only resolved once, at start-up time. If you cannot
  // handle option change well during execution, this overload is what you
  // should use.
  Option(fixed_option_t, const std::string& provider, option::Key name,
         T default_value = T(), Function<bool(T)> validator = nullptr);

  // For dynamic options, they can change at any time (calling `Get()`
  // concurrently is safe, don't worry), you must be prepared to handle value
  // change.
  Option(dynamic_option_t, const std::string& provider, option::Key name,
         T default_value = T(), Function<bool(T)> validator = nullptr);

  // If neither `fixed_option` nor `dynamic_option` is given to constructor, by
  // default a "dynamic" one is constructed.
  Option(const std::string& provider, option::Key name,
         T default_value = T(), Function<bool(T)> validator = nullptr);

  // Were reference returned, it's valid until next call to `Get()`.
  value_or_ref Get() const noexcept;

  // Implicitly convertible to `value_or_ref`. @sa `Get()` for lifetime of the
  // return value.
  /* implicit */ operator value_or_ref() const noexcept;
};

// Comparision operators are overloaded for comparing `Option<T>` with other
// types (in the same way as how `T` is compared with others.).
//
// ...

// Right-shift operator is overloaded for stream I/O.
//
// ...
```

通常而言`Option<T>`被定义在命名空间作用域（即全局变量）以被反复使用（下述各种`XxxOption`同）。将之定义为临时变量会导致反复构造释放，**反复构造`Option<T>`会造成明显的性能损耗。**

使用示例：

```cpp
Option<bool> crash_randomly("my_provider", "eval_flag_crash_randomly");  // Namespace scope.

void HarmlessMethod() {
  // `crash_randomly` is implicitly converted to `bool` here.
  if (crash_randomly && (Random() % 1000000 < 5)) {  // Do NOT crash too often.
    std::terminate();
  }
}
```

`Option<T>::Get()`及隐式转换到`const T&`均（及下述各种`XxxOption`，有特殊说明除外）[高度优化，性能很好](../base/option_benchmark.cc)，可以任意访问无需担心性能问题。

*为了保证性能，配置的值是后台异步更新的。更新频率可以通过参数`--flare_option_update_interval`配置。*

**手动复制一份`Option<T>`来企图加速实际上反而可能会导致性能下降（尤其是对于复制成本较高的`std::string`）。**

```cpp
XOption<std::string> my_flag1("group1", "key1");

void Bad() {
  // DON'T DO THIS. THE COPY HURTS PERFORMANCE.
  std::string flag_cp = my_flag1;
  if (flag_cp == "1") {
    // ...
  } else if (flag_cp == "2") {
    // ...
  }
}

void Good() {
  // This is the encouraged way.
  if (my_flag1 == "1") {
    // ...
  } else if (my_flag1 == "2") {
    // ...
  }
}
```

目前我们提供了如下几种后端实现：

## GFlags

GFlags的后端实现主要用作测试，其对应的类型是`GflagsOptions<T>`。构造函数与`Option<T>`类似，但`provider`被默认为`"gflags"`。

**我们不推荐在除测试用途之外的任何场景下使用`GflagsOptions<...>`。**

需要注意的是，由于我们是定期轮询Flags的值，因此在修改了`FLAGS_xxx`之后，在`GflagsOptions<...>::Get`变化之前，至多会有数十秒的延迟。

```cpp
DEFINE_uint32(my_flag, 1, "");

GflagsOptions<std::int32_t> fancy_my_flag("my_flag");

void GreatMethod() {
  FLARE_CHECK_EQ(FLAGS_my_flag, fancy_my_flag);
}
```
## 运行期确定配置名

某些情况下可能无法在编译期间确定要读取的配置的键名，这种情况下有两种解决方式：

- 运行期间动态创建`XxxOption<T>`。这不但允许动态确定配置名，也允许动态增加需要读取的配置项。

- 使用`option::DynamicKey`或`option::ReferencingKey`指定配置的键名。

  - `option::DynamicKey`构造时接受一个“key名”，之后可以使用`option::SetDynamicKey(...)`来为这个“key名”指定一个实际的值。这个实际的值将用来查询配置管理系统。

    需要注意的是`option::SetDynamicKey(...)`之后，等到下次后台刷新配置时才会实际更新。

  - `option::ReferencingKey`主要用于引用类似于GFlags等（框架调用`option::InitializeOptions()`之前即被初始化的）全局变量，在运行期间被引用的变量**不**应当被修改，否则可能会由于data-race导致崩溃。如果需要运行过程中动态修改，可以使用`option::DynamicKey`。

作为一个例子，假设在X配置系统中针对不同的集群分别配置了`my fancy key`，如下代码介绍了如何在运行期间动态的切换使用的配置对应的集群：

```cpp
XOption<int> some_opt(option::DynamicKey("set_name"), "my fancy key");

// Call by someone either synchronously or asynchrounously.
void SomeoneSettingDynamicKey() {
  option::SetDynamicKey("set_name", "huge set");
  option::SynchronizeOptions();  // Synchronizes options immediately. Usually
                                 // you don't need to call this as the options
                                 // are synchronized periodically.
}

void FancyMethod() {
  // You must have called `SomeoneSettingDynamicKey()` at least once (calling it
  // before starting your server may be a good choice) before touching the
  // option.
  if (some_opt == 10) {
    // ...
  }
}
```

## 手动刷新配置

某些情况下可能需要*立即*获取最新的配置，这可以通过调用`option::SynchronizeOptions()`实现。

## 用户自定义类型支持

`XxxOption<RawType, Parser>`允许用户指定一个如下形式的转换类，在本地将配置中心的配置数据做必要的类型转换（如`std::string` -> `Json::Value`）：

```cpp
struct SomeParer {
  static std::optional<SomeType> TryParse(const RawType& value);
};
```

`XxxOption>RawType, SomeParser>`在检测到配置更新后，会尝试使用`SomeParser`解析`RawType`。如果解析失败则不会更新本地配置，如果解析成功则用解析后的结果更新本地配置。

```cpp
Option<std::string, StringToJsonParser> some_config;

void FancierMethod() {
  // Get JSON value.
  if (some_config.Get()["json_key"].asInt() == 10) {
    // ...
  }
}
```

我们为常见的类型解析提供了内置的`Parser`；

- [`std::string` -> `Json::Value`](../base/option/json_parser.h)
- [从 debug-string 解析 Protocol Buffers 消息](../base/option/proto_parser.h)

---
[返回目录](README.md)
