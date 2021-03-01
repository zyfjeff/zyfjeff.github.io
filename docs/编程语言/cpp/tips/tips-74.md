---
hide:
  - toc        # Hide table of contents
---
# Tip of the Week #74: Delegating and Inheriting Constructors

> Originally posted as totw/74 on 2014-04-21
> By Bradley White (bww@google.com)
> “Delegating work works, provided the one delegating works, too.” – Robert Half

当一个类有多个构造函数的时候，通常这些构造函数变体中会执行相似的初始化逻辑，为了避免代码冗余，许多较旧的类采用在构造函数中调用私有的`SharedInit()`方法来初始化，例如:

```cpp
class C {
 public:
  C(int x, string s) { SharedInit(x, s); }
  explicit C(int x) { SharedInit(x, ""); }
  explicit C(string s) { SharedInit(0, s); }
  C() { SharedInit(0, ""); }
 private:
  void SharedInit(int x, string s) { … }
};
```

`C++11`提供了一种新的机制，委托构造，通过允许根据另外一个构造函数来定义一个新的构造函数来清晰地处理这种情况。并且对于那些成员的默认初始化成本非常昂贵的场景也会带来性能上的提升

```cpp
class C {
 public:
  C(int x, string s) { … }
  explicit C(int x) : C(x, "") {}
  explicit C(string s) : C(0, s) {}
  C() : C(0, "") {}
};
```

需要注意的是，如果你委托了另外一个构造函数就没有办法使用成员初始化列表了，所有的初始化动作都需要在委托的构造函数中完成，如果所有共享代码都是set成员，则单独的成员初始化列表或类内初始值设定项可能比使用委托构造函数更清晰。善用判断力。

>在委托构造函数返回之前，对象不被视为完整的。在实践中仅在构造函数可以抛出异常的时候，才会起作用。因为它们将委托的对象保留为不完整的状态。

此外，对于含有多个构造函数的类来说，其派生类如何在避免构造函数代码不重复的情况下进行构造呢?，比如，考虑一个类C的派生类D，这个类仅仅添加了一个新的成员函数。

```cpp
class D : public C {
 public:
  void NewMethod();
};
```

D如何进行构造呢?我们想简单地重用C中的那些，而不是写出所有的转发样板，C++11通过新的继承构造函数机制实现了这一点。

```cpp
class D : public C {
 public:
  using C::C;  // inherit all constructors from C
  void NewMethod();
};
```

通过`using C::C`这种新的形式，就可以重用类C所有的构造函数。

但请注意，当派生类不添加需要显式初始化的新数据成员时，只应继承构造函数。实际上，样式指南警告不要继承构造函数，除非新成员（如果有）具有类内初始化。
因此，继续使用`C++11`的委托和继承构造函数，它们可以减少重复，消除转发样板，或者使您的类更简单，更清晰。