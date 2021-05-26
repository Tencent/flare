# RTTI

对于处于正常运行流程中的RTTI需求，C++内置的RTTI可能引入不必要的开销，因此我们参考[LLVM的实现](https://llvm.org/docs/HowToSetUpLLVMStyleRTTI.html)，实现了一套RTTI机制。

## 约定

为使现有的类支持我们的RTTI接口，其需要通过如下几种方式之一（但**不应同时**）向框架提供必要的信息：

- 如果向下转换时始终转换到*精确匹配*的运行时类型（即*向下转换时*从不转换到某个对象的父类），那么可以让`T`从`ExactMatchCastable`继承，并在构造时调用`SetRuntimeType(kRuntimeType<T>)`即可。*`SetRuntimeType`通过ADL找到，不应当通过`flare::`指定。*

  *这种情况下如果需要向下转换至实际运行时类型的基类，依然可以通过`dynamic_cast`，或者对转换目标（运行时类型的基类）特化`CastingTraits<T>`实现。但是**这种用法目前不受支持，如果有这种任意转换的需求，我们建议使用更加通用的其他方法**。*

- 每个子类均提供原型如`bool T::classof(const Base& val)`的*静态方法*。这一方法应当使用类型自身所提供（框架不感知）的途径，来识别`val`实际类型是否为`T`**或其子类**。对于某些场景，可能可以通过继承`Castable`基类，通过其提供的`SetRuntimeType`、`GetRuntimeType`、`kRuntimeType<T>`来简化实现。

- 特化`CastingTraits<T>`，并实现`bool RuntimeTypeCheck<U>(const U& val)`。其行为同`T::classof`。

*第二种方式侵入型较强但编码简单，第三种方式优缺点相反。*

**对于编译期即可确定可行的*向上转换*（即子类转换为基类的情况），框架会进行优化，这种情况`classof`无需处理（因此基类不需要提供`classof`）。**

### 示例

这儿我们通过给基类增加`type`字段来实现运行时识别类型，取决于具体的场景，用户也可以通过其他方式（如虚函数等）来识别运行时类型。

*相对来说，增加字段来识别运行时类型提供了更多的优化空间，通过被内联的accessor（如这儿示例）访问`type`可以被优化为单条[`mov`](https://www.felixcloutier.com/x86/mov)指令；而虚函数通常可能会引入难以优化（[devirtualization](http://hubicka.blogspot.com/2014/01/devirtualization-in-c-part-1.html)）的虚函数调用。*

#### 通过`ExactMatchCastable`实现

```cpp
struct C1 : ExactMatchCastable {};
struct C2 : C1 {
  C2() { SetRuntimeType(this, kRuntimeType<C2>); }
};
struct C3 : C1 {
  C3() { SetRuntimeType(this, kRuntimeType<C3>); }
};

void CastTypes() {
  auto pc2 = std::make_unique<C2>();
  C1* p = pc2.get();
  ASSERT_NE(nullptr, dyn_cast<C1>(p));  // Success.
  ASSERT_NE(nullptr, dyn_cast<C2>(p));  // Success.
  ASSERT_EQ(nullptr, dyn_cast<C3>(p));  // Failure.
}
```

#### 通过`classof`实现

```cpp
struct Base {
  enum { kA, kB, kC } type;
};

struct A : Base {
  A() { type = kA; }

  static bool classof(const Base& val) {
    return val.type == kA || val.type == kB;
  }
};

struct B : A {
  B() { type = kB; }

  static bool classof(const Base& val) { return val.type == kB; }
};

struct C : Base {
  C() { type = kC; }

  static bool classof(const Base& val) { return val.type == kC; }
};

void CastTypes() {
  auto pb = std::make_unique<B>();
  ASSERT_NE(nullptr, dyn_cast<Base>(pb.get()));  // Success.
  ASSERT_NE(nullptr, dyn_cast<A>(pb.get()));     // Success.
  ASSERT_NE(nullptr, dyn_cast<B>(pb.get()));     // Success.
  ASSERT_EQ(nullptr, dyn_cast<C>(pb.get()));     // Failure.
}
```

## 接口

我们提供了如下一些方法供使用：

- `bool isa<T>(const U& val)`：判定`val`的运行时类型是否为`T`或其子类。
- `T* dyn_cast<T>(U& val)` / `T* dyn_cast<T>(U* val)`：如果`val`运行时类型是`T`或其子类，则将之转换为`T*`，否则返回`nullptr`。**`dyn_cast`不能接受`nullptr`**。
- `T* cast<T>(U& val)` / `T* cast<T>(U* val)`：将`val`转换为`T*`。如果`val`的运行时类型不是`T`或其子类，则会导致`CHECK`失败（即崩溃）。**`cast`不能接受`nullptr`。**
- `T* (dyn_)cast_or_null(...)`：同对应的`(dyn_)cast`，但可以接受`nullptr`（并在这种情况下返回`nullptr`）。

### 接口设计考量

LLVM的接口设计与[`dynamic_cast`](https://en.cppreference.com/w/cpp/language/dynamic_cast)有一些不同。我们经过分析，认为LLVM的选择是合理的，并采取了相同的设计：

- `(dyn_)cast`默认不支持`nullptr`。

  `dynamic_cast`显式允许传入`nullptr`，但是需要转换可能为`nullptr`的情况实际上较少，因此我们单独提供了`..._or_null`版本处理这种情况。

- 模板参数只需要提供具体类型`T`，而非指针或引用类型。

  `dynamic_cast`接受`T*`或`T&`并：

  - 由之决定表达式类型；
  - 根据类型决定转换失败返回`nullptr`（既失败符合预期）或抛出异常（即失败属于错误）。

  `dynamic_cast`的这一设计存在几方面问题：

  - C++标准在这儿难以保持一致，如模仿`dynamic_cast`的[`dynamic_pointer_cast`](https://en.cppreference.com/w/cpp/memory/shared_ptr/pointer_cast)接受的是`T`，而非`xxx_pointer<T>`（即，最终返回类型）。
  - 模板参数导致了对转换失败是否是“错误”的定义的不一致；
  - 增加编码量（`T` vs `T*` / `T&`）。

  另外，只接受参数`T`允许我们始终返回`T*`，这更符合我们的编码规范（不使用非常量引用，只使用非常量指针）。

- 针对转换是否失败符合预期，分别提供`dyn_cast`或`cast`。`dynamic_cast`通过`T*`或`T&`区分，

  我们的分析见上。

## 性能对比

我们在Xeon Gold 6133上的[测试代码](../base/casting_benchmark.cc)得到如下数据：

```text
Benchmark                                    Time           CPU Iterations
--------------------------------------------------------------------------
Benchmark_BuiltinDynamicCast                22 ns         22 ns   31094959
Benchmark_DynCast                            2 ns          2 ns  419860704
Benchmark_ExactMatchCastableDynCast          2 ns          2 ns  349577035
```

*我们的benchmark框架空转开销约2ns，因此`dyn_cast`本身的开销应当小于1ns。*

我们分析了编译产出，确认`dyn_cast`的调用没有被优化掉，并编译为如下代码：

```asm
... <+48>: mov    (%rax),%rax
... <+51>: cmpl   $0x2,0x8(%rax)
... <+55>: cmovae %r12,%rax
... <+59>: mov    %rax,(%rdx)
```

---
[返回目录](README.md)
