## Tip of the Week #55: Name Counting and unique_ptr

> Originally published as totw/55 on 2013-09-12
>
> *by Titus Winters (titus@google.com)*
>
> Updated 2017-10-20
>
> Quicklink: [abseil.io/tips/55](https://abseil.io/tips/55)
>
> *“Though we may know Him by a thousand names, He is one and the same to us all.” - Mahatma Gandhi*

通俗来说，值的"名称"可以是任何值类型的变量(不是指针，也不是引用)，在任何范围内，它包含特定的数据值(严格来说，这里讨论的"名称"指的是左值)。`std::unique_ptr`比较特殊，它要求存入的值只能有一个对应的名称，

值得注意的是，C++语言委员会，会为`std::unique_ptr`选择了一个非常合适的名称。存储在`std::unique_ptr`中任何非空指针的值必须在任何时候只出现在一个`std::unique_ptr`中。标准库在设计的时候就强制了这个行为。`std::unique_ptr`代码相关的许多编译问题都可以通过学习如何识别`std::unique_ptr`的名称计数来解决: 计数是1是正常的，但是同一指针值有多个名称就有问题了。

让我们来统计一下名字吧，在每一行的地方，计算这个点和`std::unique_ptr`所包含的指针相同的名称有几个(无论是否在范围内).如果你有发现有多个名称的值为相同指针的话。那就是错误。

```cpp
std::unique_ptr<Foo> NewFoo() {
  return std::unique_ptr<Foo>(new Foo(1));
}

void AcceptFoo(std::unique_ptr<Foo> f) { f->PrintDebugString(); }

void Simple() {
  AcceptFoo(NewFoo());
}

void DoesNotBuild() {
  std::unique_ptr<Foo> g = NewFoo();
  AcceptFoo(g); // DOES NOT COMPILE!
}

void SmarterThanTheCompilerButNot() {
  Foo* j = new Foo(2);
  // Compiles, BUT VIOLATES THE RULE and will double-delete at runtime.
  std::unique_ptr<Foo> k(j);
  std::unique_ptr<Foo> l(j);
}

```
在`Simple`函数中，`NewFoo()`分配了唯一的指针，可以通过`AccepFoo()`中的名称`f`来引用了它。

`DoesNotBuild`和`Simple`相反，`NewFoo()`分配的指针有两个名称引用它，一个是`g`,另外一个是`AccepFoo`中的`f`。

这是一个经典的唯一性违规: 在执行的任何给定点，`std::unique_ptr`(或者更一般来说，任何只移动类型)持有的任何值只能有一个不同的名称引用。任何看起来像是通过拷贝引入了名称副本的行为都是禁止的，不会进行编译。

```cpp
scratch.cc: error: call to deleted constructor of std::unique_ptr<Foo>'
  AcceptFoo(g);
```
即使你通过了编译，`std::unique_ptr`的运行时仍然会检测到这种行为。任何时候你给`std::unique_ptr`引入了多个名称，你以为骗过了编译器(看`SmarterThanTheCompilerButNot`函数)，编译没有问题，但你会遇到运行时的内存问题。

现在问题变成了，我们如何删除名称? `C++11`中通过`std::move`的形式为它提供了解决方案。

```cpp
 void EraseTheName() {
   std::unique_ptr<Foo> h = NewFoo();
   AcceptFoo(std::move(h)); // Fixes DoesNotBuild with std::move
}
```

`std::move`的调用，实际上就是一个名称擦除: 感念上来讲，你可以停止对`h`进行名称引用的计数了。现在上面的代码通过了名称唯一性的原则了，通过`NewFoo`分配的唯一性指针只有一个名称`h`，调用`AcceptFoo`的时候同样也只有一个名称引用它，因为通过`std::move`已经把名称`h`给擦除了。

名称计数是现代C++中一个很方便的技巧，对于那些不熟悉左值和右值的人来说，这个机制可以帮助你识别不必要的拷贝，帮助你正确使用`std::unique_ptr`。如果你发现某个值有太多的名称，请使用`std::move`删除不需要的名称。
