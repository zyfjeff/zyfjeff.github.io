## Tip of the Week #77: Temporaries, Moves, and Copies

> Originally published as totw/77 on 2014-07-09
> By Titus Winters (titus@google.com)
> Updated 2017-10-20
> Quicklink: abseil.io/tips/77

在不断尝试弄清楚如何向 non-language-lawyers 的人解释 C++11是如何改变事物的过程中，我们提出了"何时复制"的系列文章， 本文是这个系列中的一部分，本文试图简化C++中何时包含副本的规则，并用一组更简单的规则进行替换。

## Can You Count to 2?

您可以? 真棒，请记住，"名称规则"意味着您可以分配给某个资源的每个唯一名称会影响该对象的存在的副本数量(可以参考[ToW 55](https://abseil.io/tips/55)关于名称计数部分)

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

这个有点令人惊讶: `C++11`认识到如果你不能再引用一个名字，那么你也不会去关心那个数据了。通过`return`可以很容器识别出这种情况。

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

## Wait, std::move Doesn’t Move?

是的，虽然我们看到调用了`std::move`，但是实际上并不是移动自己，仅仅是将其转换为右值引用的类型。然后另外一个变量通过移动构造或者移动赋值来接收。

```cpp
std::vector<int> foo = GetSomeInts();
std::move(foo); // Does nothing.
// Invokes std::vector<int>’s move-constructor.
std::vector<int> bar = std::move(foo);
```

## Aaaagh! It’s All Complicated! Why!?!