## Tip of the Week #11: Return Policy

> Originally posted as TotW #11 on August 16, 2012
> by Paul S. R. Chisholm (p.s.r.chisholm@google.com)
> Frodo: There’ll be none left for the return journey. Sam: I don’t think there will be a return journey, Mr. Frodo. – The Lord of the >Rings: The Return of the King (novel by J.R.R. Tolkien, screenplay by Fran Walsh, Philippa Boyens, & Peter Jackson)

**注意**: 这条建议虽然仍然是相关的，但这是在C++11引入移动语义之前。请记住[Totw #77](https://abseil.io/tips/77)中提到的建议。

许多老的C++代码库表现出来很惧怕对象复制。高兴的是，我们可以进行对象复制，但是实际上并没有复制，这要感谢所谓的"[Return value optimization](https://en.wikipedia.org/wiki/Return_value_optimization)"(RVO)机制了。

`RVO`是一个所有编译器都长期支持的特性。考虑下面这段`C++98`的代码，它具有拷贝构造函数和一个赋值操作符，这些函数的调用开销很大，开发者们让他们每次使用都打印一条消息。
```cpp
class SomeBigObject {
 public:
  SomeBigObject() { ... }
  SomeBigObject(const SomeBigObject& s) {
    printf("Expensive copy …\n", …);
    …
  }
  SomeBigObject& operator=(const SomeBigObject& s) {
    printf("Expensive assignment …\n", …);
    …
    return *this;
  }
  ~SomeBigObject() { ... }
  …
};
```
(**注意**: 在这里我们故意避免去讨论移动操作，可以看[TotW #77](https://abseil.io/tips/77)获取更多的信息。)

如果这个类有如下这样的方法，你是否会惊恐的退缩?
```cpp
static SomeBigObject SomeBigObjectFactory(...) {
  SomeBigObject local;
  ...
  return local;
}
```
看起来很不高效，是吗? 如果我们运行下面的代码会发生什么?
```cpp
SomeBigObject obj = SomeBigObject::SomeBigObjectFactory(...);
```

理论上来说: 你可能预期至少有两个对象被创建，一个是从函数调用处返回的对象，另外一个是调用函数中的对象，两者都会进行复制，因此程序会打印两条`Expensive`操作的消息。实际上没有任何消息输出，因为拷贝构造和赋值构造没有被调用。

这是怎么发生的? 很多有经验的C++程序员会先创建一个对象，然后将这个对象的地址传递给函数，函数内部会通过地址或者引用来操作传递进来的对象。这样就可以避免不必要的对象拷贝了，这是高效的。编译器可以将不高效的代码转换为上述描述的高效代码。

当编译器看到函数调用出的一个变量(将会根据返回值来进行构造)和被调用函数中的变量(将被返回)时，它意识到不需要两个变量就可以完成这个操作。在能覆盖的场景下，编译器会将函数调用返回处的变量其地址传递给被调用的函数。

通过查看C++98的标准可以得知，“每当使用拷贝构造函数来复制临时对象的时候...允许将其实现为原始变量和副本视为引用同一个对象的两种不同的方式，而不是执行拷贝。即使类的复制构造函数或析构函数有副作用。对于具有类返回类型的函数来说，如果return语句中的表达式是本地对象的名称...，允许在实现的时候省略临时对象的创建以保存函数返回...”（第12.8节[class.copy] C++98标准的第15段.C++11标准在第12.8节第31段中有类似的语言，但它更复杂。）

标准中提到"允许"这个关键字，也就是表明这并不是强烈保证的，幸运的是所有的现代C++编译器都会默认执行`RVO`，即使是在`debug`构建、`non-inlined`函数中。

### How Can You Ensure the Compiler Performs RVO?
被调用的函数中应该为返回值定义一个变量：

```cpp
SomeBigObject SomeBigObject::SomeBigObjectFactory(...) {
  SomeBigObject local;
  …
  return local;
}
```

函数的调用处应该将返回值赋值给一个新的变量

```cpp
// No message about expensive operations:
SomeBigObject obj = SomeBigObject::SomeBigObjectFactory(...);
```

如果调用的函数重用了现存的变量来存储返回值的话(虽然在这种情况下是可以使用移动语义来完成)，编译器就无法做`RVO`了

```cpp
// RVO won’t happen here; prints message "Expensive assignment ...":
obj = SomeBigObject::SomeBigObjectFactory(s2);
```

如果调用的函数内部使用了一个或者多个变量用做返回值的时候，编译器同样无法执行RVO。

```cpp
// RVO won’t happen here:
static SomeBigObject NonRvoFactory(...) {
  SomeBigObject object1, object2;
  object1.DoSomethingWith(...);
  object2.DoSomethingWith(...);
  if (flag) {
    return object1;
  } else {
    return object2;
  }
}
```
但是如果调用的函数内部使用一个变量在多个位置返回的时候是ok的。

```cpp
// RVO will happen here:
SomeBigObject local;
if (...) {
  local.DoSomethingWith(...);
  return local;
} else {
  local.DoSomethingWith(...);
  return local;
}
```
这可能是您需要了解的有关的`RVO`的所有信息了。

### One More Thing: Temporaries
`RVO`可以工作在临时变量，而不仅仅是命名的变量，当被调用函数返回对象时，您可以从`RVO`中收益:

```cpp
// RVO works here:
SomeBigObject SomeBigObject::ReturnsTempFactory(...) {
  return SomeBigObject::SomeBigObjectFactory(...);
}
```

当函数调用的地方立即使用函数的返回值(它存储在临时对象中)，你同样可以从`RVO`中收益。

```cpp
// No message about expensive operations:
EXPECT_EQ(SomeBigObject::SomeBigObjectFactory(...).Name(), s);
```

**最后需要注意的**: 如果你的代码需要拷贝，那么就拷贝，无论拷贝的副本是否可以优化。不要为了效率还失去正确性。