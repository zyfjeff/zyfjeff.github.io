## Tip of the Week #74: Delegating and Inheriting Constructors

> Originally posted as totw/74 on 2014-04-21
> By Bradley White (bww@google.com)
> “Delegating work works, provided the one delegating works, too.” – Robert Half

当一个类有多个构造函数的时候，通常这些构造函数变体中会执行相似的初始化逻辑，为了避免代码冗余，许多较旧的类采用在构造函数中
调用私有的`SharedInit()`方法来初始化，例如:

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

`C++11`提供了一种新的机制，委托构造，通过允许根据另外一个构造函数来定义一个构造函数来清楚地处理这种情况。

