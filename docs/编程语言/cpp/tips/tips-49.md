## Tip of the Week #49: Argument-Dependent Lookup

> Originally posted as totw/49 on 2013-07-14
>
> *“…whatever disappearing trail of its legalistic argle-bargle one chooses to follow…” –Antonin Scalia, U.S. v Windsor dissenting opinion*

### Overview

一个函数调用表达式如`func(a,b,c)`，其中函数被命名为不使用`::`操作符的，这被称为未限定名称。当`C++`代码引用一个未限定名称的函数时，编译器会去执行匹配函数声明的搜索。令人有些人惊讶的是(与其他语言不同)，除了在调用者的作用域内查找外，搜索的范围还包括了函数参数类型相关联的名称空间，这种额外的查找被称为`Argument-Dependent Lookup (ADL)`，这绝对是在你的代码中发生的，所以你会更好的理解它是如何工作的。

### Name Lookup Basics

一个函数调用必须映射到一个编译器定义的的函数。这种匹配关系的映射是在两个独立的串行处理阶段完成的，第一步就是名称解析，应用于一些作用域的搜索规则去产生一系列可以匹配的函数名，而重载决策采用该名称查找产生的重载，并尝试选择一个和调用处传入的参数最匹配的函数名称。记住这个区别，名称解析是第一步，它并不试图确定函数是否匹配，它甚至不考虑参数，它只是在作用域内搜索函数名称，重载解析本身就是一个复杂的话题，但现在不是我们的重点。只要知道它是一个单独的处理阶段，它可以从名称查找中获取输入。

当遇到一个未限定函数调用时，该函数名称可能会出现几个独立的搜索序列，每一个搜索序列都会试图匹配一系列的重载名称。最明显的搜索序列就是从调用点的作用域开始处开始向外搜索。

```c++
namespace b {
void func();
namespace internal {
void test() { func(); } // ok: finds b::func().
} // b::internal
} // b
```

这个名称查找与ADL无关(`func`没有参数)，它只是简单的从函数调用处向外搜索。从本地函数作用域向外进行，到类作用域、封闭类作用域和基类，然后到命名空间作用域，并进入到封闭命名空间，最后到全局命名空间。

名称查询通过一系列日益扩大的范围进行着，只要找到具有目标名称的任何就会停止进行搜索。无论该函数的参数是否与调用点提供的函数函数兼容。当遇到包含至少一个具有目标名称的函数声明的作用域时，该作用域中的重载决策将成为该名称查找的结果。

这在下面的例子中有说明：

```c++
namespace b {
void func(const string&);  // b::func
namespace internal {
void func(int);  // b::internal::func
namespace deep {
void test() {
  string s("hello");
  func(s);  // error: finds only b::internal::func(int).
}
}  // b::internal::deep
}  // b::internal
}  // b
```

上面的例子会让人很困惑认为`func(s)`表达式会忽略掉`void func(int)`，并继续到下一个作用域中寻找到`b::func(const string&)`。然而名称解析的时候并不会考虑到参数类型，它仅仅找那些函数名字叫`func`的并最终在`b::internal`这个作用域中找到并停下了。结果就是将一个明显不好的匹配交给了重载决策阶段来评估了。最终`b::func(const string&)`这个函数也没有出现在重载决策的阶段。

作用域搜索顺序的一个重要含义是，搜索顺序中较早出现的作用域中的重载将会隐藏后面的作用域中的重载。

### Argument-Dependent Lookup

如果一个函数调用传递了函数那么几个并行的名称查找过程都会同时进行，这些额外的查找是从这个函数的每一个调用参数所在的命名空间中开始。当遇到名称匹配的作用域时并不会停止查找，只有遇到匹配的那一个才会结束。

### The Simple Case

考虑下面这段代码：

```c++
namespace aspace {
struct A {};
void func(const A&);  // found by ADL name lookup on 'a'.
}  // namespace aspace

namespace bspace {
void func(int);  // found by lexical scope name lookup
void test() {
  aspace::A a;
  func(a);  // aspace::func(const aspace::A&)
}
}  // namespace bspace
```

上面的代码中在调用`func(a)`时会存在两个名称查找的过程，一个是从`bspace::test()`所在的作用域开始向外进行名称的查找，当没有发现有任何匹配的名称时就开始在`bspace`所在作用域中进行查找，并发现了`func(int)`于是停止了查找。另外一个名称查找的过程是`ADL`，它是从函数的调用参数`a`所在的namespace开始，这上面的例子中就是`aspace`，在这个namespace中找到了`aspace::func(const aspace::A&)`并停止。最后将这两个匹配到的函函数交给重载决策，在重载决策阶段会根据参数进行最佳匹配，最后寻找到的最佳匹配就是`aspace::func(const aspace::A&)`，而`bspace::func(int)` 在重载阶段发现其参数并不匹配所以被拒绝了。

基于调用处所在作用域开始的名称查找和每个因为`ADL`所触发的名称查找可以被认为是并行发生的，每一个搜索都会返回一组候选的函数重载。所有的这些搜索所产出的结果都会放在一个集合中，最后通过重载决策阶段以确定最佳的匹配。如果有一批最佳的匹配，那么编译器就会发出一个模糊的错误 "只能有一个最佳的匹配"，如果没有找到任何一个最佳的匹配，编译器也会报出一个错误 "必须要有一个匹配"。

### Type-Associated Namespaces

前面的例子是一个比较简单的例子，更复杂的类型可以有多个与之关联的`namespace`，这个与之关联的`namespace`包括了与这个类型相关的任何`namespace`，参数类型的全称所在的`namespace`就是其中的一部分，此外还有模版参数的类型所在的`namespace`，还包括了其直接或间接的父类所在的`namespace`。例如一个单参数的函数调用`a::A<b::B, c::internal::C*>`将会产生`a`、`b`、`c::internal`等三个搜索的域，在每一个搜索的域中都会查找和调用函数名称相同的函数。下面这个例子就显示了这些效果：

```c++
namespace aspace {
struct A {};
template <typename T> struct AGeneric {};
void func(const A&);
template <typename T> void find_me(const T&);
}  // namespace aspace

namespace bspace {
typedef aspace::A AliasForA;
struct B : aspace::A {};
template <typename T> struct BGeneric {};
void test() {
  // ok: base class namespace searched.
  func(B());
  // ok: template parameter namespace searched.
  find_me(BGeneric<aspace::A>());
  // ok: template namespace searched.
  find_me(aspace::AGeneric<int>());
}
}  // namespace bspace
```

### Tips

随着基本的名称查找机制在你的脑海中开始记忆犹新后，请考虑下面这些`Tips`，这些`Tips`可能会帮助您写出更佳的`C++`代码。

### Type Aliases

有的时候要确定一个类型所关联的`namespace`是需要花一些时间来辨别的。`typedef`和`using`声明可以给一个类型引入别名。在这些情况下，选择要搜索的`namespace`列表之前需要将这些别名进行解析，并扩展为他们的源类型。这是`typedef`和`using`声明可能会带来的一些误导，因为他们可能会导致您在对ADL需要搜索哪些`namespace`时会进行不正确的预测。如下所示：

```c++
namespace cspace {
// ok: note that this searches aspace, not bspace.
void test() {
  func(bspace::AliasForA());
}
}  // namespace cspace
```

### Caveat Iterator

对迭代器要小心。你不知道他们关联的是什么`namespace`，所以不要依赖于`ADL`来解决涉及迭代器的函数调用的名称解析。它们可能只是指向元素的指针，或者可能是在一个与容器实现所在的`namespace`无关的一个私有`namespace`中。

```c++
namespace d {
int test() {
  std::vector<int> vec(a);
  // maybe this compiles, maybe not!
  return count(vec.begin(), vec.end(), 0);
}
}  // namespace d
```

上面的代码依赖于`std::vector<int>::iterator`是`int*`(这是可能的)，还是某个类型在具有`count`重载函数的命名空间中(如`std::count()`)。这可能会在某些平台上是可以运行的，而在其他平台上则无法运行，或者它可以在`debug`下可以正常工作，而在`release`下无法运行。这种情况下使用带有限定名称的函数调用方式会更好，例如可以像这样来调用`count`函数，`std::cout()`。

### Overloaded Operators

运算符(例如 `+`或`<<` )可以被认为是一种函数名称，例如`operator+(a,b)`或`operator<<(a,b)`这些都被认为是未限定名称的调用。`ADL`最重要的应用就是用于在通过`operator<<`来记录日志的时候。通常我们会看到像`std::cout << obj;`这样的调用，对于`obj`我们假定认为它的类型是`O::Obj`。这个语句展开来看就是这样的`operator<<(std::ostream&, const O::Obj&)`，是一个未限定名称的函数调用，它将会通过`ADL`从`std::ostream`参数所对应的`std` `namespace`中进行名称的查找，以及第二个参数`0::0bj`的`0` `namespace`中进行查找，当然还会从函数调用处所在的作用域开始向外进行查找。

将这些运算符放在与他们要操作的用户自定义类型相同的名称空间中很重要：在上面的`namespace` `0`的例子中，如果将`operator<<`放在像`::`(全局`namespace`)这样的外部`namespace`中，该操作符将工作一段时间，直到有人非常无辜的将一个不相干的其他类型的`operator<<`操作符放在`namespace` `0`中

### Fundamental Types

请注意，基本类型(如`int`、`double`等)不和全局`namespace`关联。它们不关联任何`namespace`，也不会对`ADL`产生作用，指针和数组类型和他们所指向的对象和或元素类型所在的`namespace`进行关联。

### Refactoring Gotchas
如果将参数类型更改为非限定函数调用的话，会影响那些具有重载的函数调用行为。只是将一个类型移动到`namespace`中，并在旧的命名空间中使用`typedef`以实现兼容性，这是没有帮助的，实际上只会使问题更难被诊断。将类型移动到新的命名空间中时要小心。

类似的，将函数移动到新的`namespace`中，并设置好`using`声明。这意味着非限定的调用可能将不会找到他，可悲的是，他们仍然可以通过找到不同的函数重载来完成编译。当移动一个函数到新的`namespace`中时要小心。

### Final Thought
相对较少的程序员理解与函数查找相关的确切规则和极端情况，该语言规范里面包含了13页关于名称搜索包含了哪些规则、特殊情况、以及和友元函数闭包类搜索范围等，需要你额外的小心。尽管存在这些复杂性，但如果您始终牢记并行名称搜索的基本概念，那么您将有足够的基础来理解函数调用和运算符是如何解析的。现在通过本文，您将能够理解函数调用和运算符是如何选择函数声明的，并且当发生重载决议失败和名称覆盖等令人费解的构建错误时，你将更容易诊断。