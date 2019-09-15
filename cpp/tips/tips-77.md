## Tip of the Week #77: Temporaries, Moves, and Copies

> Originally published as totw/77 on 2014-07-09
> By Titus Winters (titus@google.com)
> Updated 2017-10-20
> Quicklink: abseil.io/tips/77

在不断尝试弄清楚如何向 non-language-lawyers 的人解释 C++11是如何改变事物的过程中，我们提出了"何时复制"的系列文章， 本文是这个系列中的一部分，本文试图简化C++中何时包含副本的规则，并用一组更简单的规则进行替换。

## Can You Count to 2?

您可以? 真棒，请记住，"名称规则" 意味着您可以分配给某个资源的每个唯一名称会影响该对象的存在的副本数量(可以参考[ToW 55](https://abseil.io/tips/55)关于名称计数部分)

## Name Counting, in Brief

如果你担心你的某些代码行可能会导致创建一个新副本。那么请认真看下当前要复制的数据存在多少个名称? 而这只有三种情况需要考虑:

## Two Names: It’s a Copy

这很简单，相同的数据如果有两个名字就是拷贝。

```cpp
std::vector<int> foo;
FillAVectorOfIntsByOutputParameterSoNobodyThinksAboutCopies(&foo);
std::vector<int> bar = foo;     // Yep, this is a copy.

std::map<int, string> my_map;
string forty_two = "42";
my_map[5] = forty_two;          // Also a copy: my_map[5] counts as a name.
```

## One Name: It’s a Move

这个有点令人惊讶: `C++11`认识到如果你不能再引用一个名字，那么你也不会去关心那个数据了。通过`return`可以很容易识别出这种情况。

```cpp
std::vector<int> GetSomeInts() {
  std::vector<int> ret = {1, 2, 3, 4};
  return ret;
}

// Just a move: either "ret" or "foo" has the data, but never both at once.
std::vector<int> foo = GetSomeInts();
```

另外一种方式就是通过是调用`std::move()`的方法告诉编译器这个名字已经不使用了(来自[ToW 55](https://abseil.io/tips/55)的“名字擦除器”部分)

```cpp
std::vector<int> foo = GetSomeInts();
// Not a copy, move allows the compiler to treat foo as a
// temporary, so this is invoking the move constructor for
// std::vector<int>.
// Note that it isn’t the call to std::move that does the moving,
// it’s the constructor. The call to std::move just allows foo to
// be treated as a temporary (rather than as an object with a name).
std::vector<int> bar = std::move(foo);
```

## Zero Names: It’s a Temporary

临时变量也很特别：如果你想避免复制，请避免为变量提供名称

```cpp
void OperatesOnVector(const std::vector<int>& v);

// No copies: the values in the vector returned by GetSomeInts()
// will be moved (O(1)) into the temporary constructed between these
// calls and passed by reference into OperatesOnVector().
OperatesOnVector(GetSomeInts());
```

## Beware: Zombies

上文中提到的这些(除了`std::move`本身外)希望理解起来更加直观，这是因为在C++11之前的几年里，我们都建立了比较奇怪的副本概念，对于一个没有垃圾收集器的语言来说，这种副本识别的方式
为我们提供了出色的性能和直观的理解。然后这并非没有危险，一个很大的问题就是，当值被`move`走后，剩下的是什么?

```cpp
T bar = std::move(foo);
CHECK(foo.empty()); // Is this valid? Maybe, but don’t count on it.
```

对于剩下的值，我们能做些什么? 这是一个很难的问题。对于大多数标准库的类型来说，剩下的值是 "有效的，但未指明的状态"，非标准类型通常遵循相同的规则。
最为安全的方法就是远离这些对象，你可以重新给他们赋值，或者让它们离开作用域进行析构，但是不要对其状态作出任何其他的假设。

`Clang-tidy`提供了一些`static-checking`机制可以帮助我们来捕捉出现`use-after move`的代码(`misc-use-after-move` static-checking的一种)，而静态分析永远无法捕捉所有这些
我们还是需要在代码审查中发现这类问题，并在您自己的代码中避免它们。


## Wait, std::move Doesn’t Move?

是的，虽然我们看到调用了`std::move`，但是实际上并不是移动自己，仅仅是将其转换为右值引用的类型。然后另外一个变量通过移动构造或者移动赋值来接收。

```cpp
std::vector<int> foo = GetSomeInts();
std::move(foo); // Does nothing.
// Invokes std::vector<int>’s move-constructor.
std::vector<int> bar = std::move(foo);
```

这应该几乎不会发生什么，你也不应该在这上面浪费太多精力。我值提到了， 如果`std::move`和移动构造函数之间的连接让你感到困惑.

## Aaaagh! It’s All Complicated! Why!?!

第一: 真的没那么糟，由于我们大多数值类型(包括protobuf)中都有移动操作，所以我们可以不讨论 这是副本吗? 这有效吗? 只需要数名字，两个名字，一份副本，小于两个名字的就没有副本了。
忽略副本问题，值语义更清晰，更容易推理，考虑下面这两个操作:

```cpp
void Foo(std::vector<string>* paths) {
  ExpandGlob(GenerateGlob(), paths);
}

std::vector<string> Bar() {
  std::vector<string> paths;
  ExpandGlob(GenerateGlob(), &paths);
  return paths;
}
```

这些是一样的吗？ 如果`*paths`中存在数据，那么数据是什么? 你怎么知道? 对于读者来说，值语义比输入/输出参数更容易推理，在这里，您需要考虑存在的数据发生了什么，以及
是否存在指针的所有发生了转移。
由于在处理值（而不是指针）时对生存期和用法的保证更简单，所以编译器的优化器更容易对这种样式的代码进行操作。
良好管理的值语义还可以最小化对分配器的攻击（这很便宜，但不是免费的）。一旦我们了解了移动语义是如何帮助我们消除副本的，
编译器的优化器就可以更好地解释对象类型、生命期、虚拟分派以及其他许多有助于生成更高效机器代码的问题。

由于大多数实用程序代码现在都具有移动性，我们应该停止担心副本和指针语义，而专注于编写简单易懂的代码。
请确保您理解新的规则：并不是您遇到的所有遗留接口都可以更新为按值返回（而不是按输出参数返回），因此始终会有样式的混合。当一个比另一个更合适的时候，你理解这一点很重要。