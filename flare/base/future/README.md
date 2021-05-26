# README

对外接口均列在`//flare/base/future.h`中，这份文档中将针对各个接口进行描述。

## 关于异常

基于我们的编码规范，我们不捕获异常。continuation中抛出异常将导致`std::terminate()`。

## Boxed<Ts...>

`Boxed<Ts...>`作为一个用于保存执行结果的容器，其将保存值（`Ts...`类型）或异常。

这一类型的对象同一时间仅可能、且始终应当保存着值或异常中的一种，而不会存在于第三种状态（不可能为“空”）。

这一类型的对象均由库构造，其构造函数通常不应被用户所调用。

这一类型提供如下对外接口：

- `/* see below */ Get() &`
- `/* see below */ Get() &&`
- `std::tuple<Ts...>& GetRaw() &`
- `std::tuple<Ts...>&& GetRaw() &&`
- `explicit operator bool() const`

### Boxed<...>::Get()

这一方法的行为取决于`Boxed<>`中所保存的内容。

- 当`Boxed<>`中保存着值（而非异常）时：

  - 若`Ts...`为空，`Get()`返回`void`；
  - 若`Ts...`含有恰好一个类型`T`，`Get()`返回`T`；
  - 否则`Ts...`含有至少2个类型，`Get()`返回`std::tuple<Ts...>`。

  若`*this`是左值引用，这一方法返回左值引用，若`*this`是右值引用，这一方法返回右值引用。

- 当`Boxed<>`中保存着异常时，`Get()`被调用时会将相应的异常重新抛出。

### Boxed<...>::GetRaw()

与`Boxed<...>::Get()`类似，`GetRaw()`的行为取决于`Boxed<>`中保存的内容。

- 当`Boxed<>`中保存着值时，这一方法通过类型`std::tuple<Ts...>`其所存储的值；

  取决于`*this`的类型，这一方法可能返回左值或右值引用。

- 否则这一方法将会抛出其保存的异常。

### Boxed<...>::operator bool()

`Boxed<...>`可以被转换成`bool`类型：

- 当`Boxed<...>`保存着值时，其转换为`bool`的值是`true`；
- 否则（`Boxed<...>`保存着异常），其转换为`bool`时是`false`。

## Future<Ts...>

`Future<...>`用于描述一个将在（通常是未来）某个时刻可以得到的值。

在某些情况下，如果有必要，用户也可以构造一个已经有值的`Future<>`，见后文。

由于我们主要的目标用户群体是异步编程，因此我们的`Future<>`并不提供阻塞的能力，而是允许用户通过`Future<>::Then`指定当值被提供（通常通过`Promise<>::SetValue`，见后文）的时候指定的后续逻辑（`continuation`）。

*由于`Future<>`并不提供阻塞能力，因此其的值会被`std::move`到`continuation`的参数中。此时如果我们提供了`Future<>::Get`或类似的访问值的方法，那这将和`continuation`处的`std::move`产生竞争条件。因此我们并未提供`Future<>`的访问值的成员方法。*

这一类型提供如下接口：

- `/* see below */ Then(F&&) &&`
- `Future(futurize_values_t, Us&&...)`
- `Future(futurize_tuple_t, std::tuple<Us...>)`
- `Future(T&&)`
- `Future(Future<Us...>&&)`

这一类型有如下deduction guides：

```cpp
template <class... Us> Future(futurize_values_t, Us&&...) ->
    Future<std::decay_t<Us>...>;
template <class... Us> Future(futurize_tuple_t, std::tuple<Us...>) ->
    Future<std::decay_t<Us>...>;
template <class U> Future(U&&) -> Future<std::decay_t<U>>;
```

`Future<>`自身不要求`Ts...`是`DefaultConstructible`或`CopyConstructible`的。

### Future<...>::Then(F&& continuation)

我们通过这一方法指定当`Future<>`有值之后调用的后续操作逻辑。

取决于`continuation`返回的类型，这一方法可能返回下述类型之一：

- 当`continuation`返回`Future<Us...>`时，`Future<>::Then`返回等价的`Future<Us...>`。
- 否则假设`continuation`返回的类型为`T`（不是某种`Future<>`），那么`Future<>::Then`返回`Future<T>`，新返回的`Future<T>`将在用于保存`continuation`被调用时返回的返回值。

`continuation`应当可以被接受下述两种参数之一：

- Ts...

  当`Future<>`有值之后，相应的值会被传递给`continuation`，期间可能会发生一次或多次`std::move`。

  若`Future<>`最终得到的是一个异常，那么这个异常会传递至`Future<>::Then`返回的`Future<...>`之中。

- Boxed<Ts...>

  当`Future<>`有值或异常之后，我们会将相应的值或异常通过`Boxed<>`保存并传递给`continuation`。

当两种参数均可以接受时（比如`[] (auto&&...) {}`就可以接受任何类型的参数），我们优先选择第一种。

无论`continuation`接受何种参数，若`continuation`执行过程中抛出异常且未被捕获，那么相关异常将会传递至`Future<>::Then`返回的`Future<...>`之中。

### Future(futurize_values_t, Us&&...)

这一构造函数用于构造一个已经有值的`Future<>`，其值来源于`Us&&...`。

仅当`Us&&...`可以隐式转换为`Ts...`时这一构造函数参与重载决策。

我们在`Future<>`所在的命名空间中提供了`constexpr`的变量`futurize_values`用于作为这一构造函数的第一个参数。

```cpp
auto ready_empty = Future(futurize_values);
auto ready_int = Future(futurize_values, 10);
auto ready_values = Future(futurize_values, 10, 2.0, "str"s);
```

### Future(futurize_tuple_t, std::tuple<Us...>)

类似于`Future(futurize_value_t, Us&&...)`，但是这一方法构造的`Future<>`的值来源于`std::tuple<...>`。

仅当`Us&&...`可以隐式转换为`Ts...`时这一构造函数参与重载。

### Future(Future<Us...>&&)

这一构造函数用于在模板参数中的类型兼容的情况下进行`Future<...>`之间的相互转换。

仅当`Us&&...`可以隐式转换为`Ts...`时这一构造函数参与重载。

### Future(T&&)

等价于`Future(futurize_values, T&&)`。

```cpp
auto ready_int = Future(10);
```

### 其他

一方面我们提供了`MakeReadyFuture`，另一方面我们也提供了必要的deduction guides。因此，我们可以使用`Future(futurize_values, [values...])`来实现类似的目的，其构造的`Future<Ts...>`的模板参数类型取决于`values...`。见上文示例。

对于`sizeof...(Ts) == 1`的情况，我们可以使用`Future(value)`来简化。

*受限于技术制约，`Future()`实际上是默认构造了一个空的`Future<>`，而不是一个已经“有值”的`Future<>`。空的`Future<>`通常只能作为占位符之作用。*

## Promise<Ts...>

为了有一种方式可以异步的在计算完成后向`Future<>`中写入值，我们通常会使用`Promise<>`。

`Promise<>`及其关联的`Future<>`（通过`Promise<>::GetFuture()`获得）内部会通过某些方式共享状态，以使得`Promise<>`可以将值或异常通过`Promise<>::SetXxx`传递给`Future<>`。

我们会在`Promise<>` / `Future<>`内部使用必要的同步机制以保证必要的线程安全。

这一类型提供如下接口：

- `Future<Ts...> GetFuture()`
- `SetValue(Us&&...)`
- `SetBoxed(Boxed<Ts...>)`

### Promise<...>::GetFuture()

这一方法返回和这一`Promise<>`相关联的`Future<>`，之后我们可以通过这个`Promise<>`的`SetXxx`系列方法将值或异常传递至`GetFuture()`返回的`Future<>`之中。

*这一方法仅应当被调用一次。*

### Promise<...>::SetValue(Us&&...)

这一方法用于将值传递至与这个`Promise<>`相关联的`Future<>`中。`Us&&...`应当可以隐式转换为`Ts...`。

### Promise<...>::SetBoxed(Boxed<Ts...>)

这一方法用于将值或异常传递至相关联的`Future<>`。

## MakeReadyFuture

这一方法用于创建一个已经含有值的`Future<>`，其具有如下原型：

```cpp
Future<decay_t<Ts...>> MakeReadyFuture(Ts&&...);
```

这一方法的模板参数应当由编译器自行推导，用户始终不应当自行指定。

```cpp
auto ready = MakeReadyFuture(10, 2.0, "str"s);  // Future<int, double, std::string>
auto ready_empty = MakeReadyFuture();  // Future<>
```

## MakeFutureWith

这一方法接受一个可以被调用的仿函数及一系列参数，并将调用这个仿函数的结果保存至返回的`Future<...>`。

这一方法具有如下原型：

```cpp
/* see below */ MakeFutureWith(F&& functor, Args&&... args);
```

取决于仿函数本身的返回类型，这一函数可能会：

- 仿函数返回`Future<...>`：这一函数原样返回仿函数的返回值；
- 仿函数返回类型`T`（不是某种`Future<...>`）：这一函数返回一个已经保存着仿函数返回值的`Future<T>`。

```cpp
Future<> vf = MakeFutureWith([] {});
Future<int> f = MakeFutureWith([] (int x) { return x; }, 10);
```

## BlockingGet

`BlockingGet`是一个自由函数，可用于阻塞等待`Future<>`有值或异常，其具有如下原型：

```cpp
/* see below */ BlockingGet(Future<Ts...>&&);  // (1)
/* see below */ BlockingGet(Future<Ts...>*);   // (2)
```

当`Future<>`最终获取到的是一个值时，这个值将会由`BlockingGet`返回，期间可能发生一次或多次`std::move`，但是我们并不会进行复制操作。

重载2与重载1行为相同，仅参数类型由右值引用换为指针类型。

这一方法的返回类型及行为与`Boxed<Ts...>::Get()`相同（见上文）。

```cpp
BlockingGet(some_void_op_async());
int result = BlockingGet(get_int_async());
std::tuple<int, double, std::string> tuple_of_values = BlockingGet(get_multiple_values_async());
auto&& [i, f, str] = BlockingGet(get_multiple_values_async());
```

*由于这一方法可能会阻塞，因此我们不建议在可能发生死锁的环境中（如线程池内）使用。*

**对于Fiber环境，我们建议使用[`fiber::BlockingGet(...)`（定义在`flare/fiber/future.h`）](../../fiber/future.h)，这样可以避免阻塞底层pthread，提升整体性能。**

## BlockingTryGet

`BlockingTryGet`的使用同`BlockingGet`，但是其接受参数`timeout`来设置等待超时。

```cpp
/* see below */ BlockingTryGet(Future<Ts...>&&, /* see below */ timeout);  // (1)
/* see below */ BlockingTryGet(Future<Ts...>*, /* see below */ timeout);   // (2)
```

这一方法的返回类型为`std::optional<...>`（如果`Ts...`不为空，其中`...`部分与`Boxed<Ts...>::Get()`的返回类型相同）或`bool`（`Ts...`为空）。

取决于具体需要，参数`timeout`可以是`std::chrono::duration<...>`或`std::chrono::time_point<...>`。

```cpp
bool f = BlockingTryGet(some_void_op_async(), 1s);
std::optional<int> opti =
    BlockingTryGet(get_int_async(), std::chrono::system_clock::now() + 1s);
std::optional<std::tuple<int, double, std::string>> tuple_of_values =
    BlockingTryGet(get_multiple_values_async(), flare::ReadySteadyClock() + 1s);
```

## Split

`Split`可以用来将一个`Future<>`“拆分”为两个，这样我们就可以在两个*所含的值*等价的`Future<>`上分别进行操作（比如调用`Future<>::Then()`）了。

这一方法具有如下原型：

```cpp
/* see below */ Split(Future<Ts...>&& future);
/* see below */ Split(Future<Ts...>* future);
```

这一方法会返回两个`Future<Ts...>`，可以通过[Structured Binding](https://en.cppreference.com/w/cpp/language/structured_binding)来将返回值绑定到两个变量上（`auto&& [f1, f2] = Split(ValueFromFuture());`）。当原始的`Future<Ts...>`得到值之后，这个值或其副本将会被复制/移动到返回的这两个`Future<Ts...>`中。

显然这一方法要求`Ts...`是`CopyConstructible`的。

```cpp
auto&& [f1, f2] = Split(ValueFromFuture());
f1.Then([] (auto&& v1) { WorkOnValue1(v); });
f2.Then([] (auto&& v1) { WorkOnValue1(v); });
```

## WhenAll

我们可以使用`WhenAll`来等待一系列`Future<>`*均*得到值或异常。

这一方法具有如下重载：

```cpp
/* see below */ WhenAll(std::vector<Future<Ts...>>&&);           // (1)
/* see below */ WhenAll(std::vector<Future<Ts...>>*);            // (2)
/* see below */ WhenAll(Future<Ts...>&&, Future<Us...>&&, ...);  // (3)
/* see below */ WhenAll(Future<Ts...>*, Future<Us...>*, ...);    // (4)
```

其中重载2、4和重载1、3行为相同，区别仅在与参数类型由右值引用改为指针。

我们返回的`Future<>`将会在传入的参数均有值或异常之后得到值或异常。若参数中的`Future<>`至少一个最终得到的是一个异常，那么返回的`Future<>`将会得到这些异常中的任意一个（我们并不保证多个异常之间的选择策略）。

对于重载1、2，我们的返回类型为：

- 若`Ts...`为空，那么返回`Future<>`；
- 若`Ts...`中仅有一个类型`T`，那么返回`Future<std::vector<T>>`；
- 否则返回`Future<std::vector<std::tuple<Ts...>>>`。

对于重载3、4，我们的返回类型为`Future<T, U, V...>`，其中`T`、`U`、`V`、...的类型分别由`Ts...`、`Us...`、`Vs...`、...按照如下策略决定：

- 若`Ts...`为空，那么相应的`T`不会出现的返回类型中；

  如`WhenAll(Future<>, Future<int>)`返回`Future<int>`，第一个`Future<>`不会出现在返回类型中。

  又如`WhenAll(Future<>, Future<>)`将会返回`Future<>`。

- 若`Us...`中仅有一个类型`U`，那么相应的我们会在返回的`Future<>`的模板参数中增加`U`；

  如`WhenAll(Future<int>)`返回`Future<int>`。

- 否则`Vs...`包含至少两个类型，那么相应的我们会在返回类型的相应位置返回`std::tuple<Vs...>`；

  如`WhenAll(Future<>, Future<int>, Future<char, double>)`返回`Future<int, std::tuple<char, double>>`。

```cpp
Future<std::vector<int>> ints = WhenAll(vector_of_future_of_ints);
Future<> empty = WhenAll(vector_of_future_of_empty);
Future<int, double, std::tuple<std::string, bool>> numbers =
    WhenAll(Future(10), Future(2.0), Future(futurize_values, "str"s, true));
```

## WhenAny

我们可以使用`WhenAny`来等待一系列的`Future<>`中的某*一个*`Future<>`得到值或异常。我们总是返回第一个得到的值或异常及其下标（仅当得到值时）。

这一方法具有如下重载：

```cpp
/* see below */ WhenAll(std::vector<Future<Ts...>>&&);  // (1)
/* see below */ WhenAll(std::vector<Future<Ts...>>*);   // (2)
```

一如以往，重载1、2的区别仅在与参数类型。

*由于我们要等待并找到有一个`Future<>`有值，因此我们要求传入的集合包含至少一个元素（不能传入空集），否则行为是未定义的。*

我们的返回类型为：

- 若`Ts...`为空集，那么我们返回`Future<std::size_t>`；
- 若`Ts...`中仅有一个类型`T`，那么我们返回`Future<std::size_t, T>`；
- 否则我们返回`Future<std::size_t, std::tuple<Ts...>>`。

```cpp
Future<std::size_t> index = WhenAny(vector_of_future_of_empty;
Future<std::size_t, int> index_and_number = WhenAny(vector_of_future_of_ints);
```

## Repeat

这一方法用于循环调用某个仿函数直到其返回`false`，其具有如下原型：

```cpp
Future<> Repeat(F&& functor);
```

其参数`functor`不应当接受任何参数，并返回`bool`类型。每当`functor`返回之后，取决于其返回值：

- 返回`true`：在未来的某个时间点`functor`会再次被调用（取决于执行器，通常可能是返回后立即、或很短的间隔之后再次被调用）；
- 返回`false`：`functor`不会再次被调用。

当`Repeat`被调用之后，`functor`即可能随时发生第一次调用。这一行为可能发生在`Repeat`返回之前。

**受限于实现，在未使用（非立即调用型的）执行器时，`functor`长时间返回`true`最终会导致这一方法栈溢出。**

```cpp
std::atomic<int> global_counter;
auto increase_counter_till_exit = Repeat([] { ++global_counter; return !!exiting_; });
```

## RepeatIf

这一方法类似于`Repeat`，具有如下原型：

```cpp
Future</* see below */> RepeatIf(F&& functor, Pred&& pred);
```

这一方法与`Repeat`是有如下区别：

- 这一方法除`functor`外，另外接受参数`pred`，每次`functor`返回值（或其返回的`Future<...>`有值）之后，其返回会被传递给`pred`，并根据`pred`返回的`bool`决定是否继续循环；
- 这一方法会返回`functor`最后一次返回的值。

```cpp
auto last_value_is_a_multiple_of_10 = RepeatIf(
    [] { return MakeReadyFuture(1, 2, rand()); },
    [] (auto, auto, auto r) { return r % 10 == 0; });
```
