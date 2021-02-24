## Tip of the Week #61: Default Member Initializers
> Originally posted as Totw #61 on Nov 12, 2013
> by Michael Chastain (mec.desktop@gmail.com)
> Updated October, 2016

### Declaring Default Member Initialization
一个默认的成员初始化声明是用于给成员在构造的时候提供一个默认值，看起来像下面这段代码:

```cpp
class Client {
 private:
  int chunks_in_flight_ = 0;
};
```
这种默认的成员初始化会自动添加到类的所有构造函数中，甚至包括C++编译器合成的构造函数。类的成员默认初始化这种方式对于拥有众多数据成员的类来说很有用，尤其是对于像`bool`、`int`、`double`和裸指针等类型。这些基本类型的非静态的数据成员经常会忘记在构造函数中进行初始化，但是任何类型的非静态数据成员都可能具有初始化程序。

> Non-static data members of any type may have initializers, though. 最后一句翻译的不好，感觉没有上下文

默认的成员初始化对于那些用户没有编写构造函数的简单结构来说，也很有用:
```cpp
struct Options {
  bool use_loas = true;
  bool log_pii = false;
  int timeout_ms = 60 * 1000;
  std::array<int, 4> timeout_backoff_ms = { 10, 100, 1000, 10 * 1000 };
};
```

### Member Initialization Overrides
如果对一个已经有默认初始化值的数据成员在构造函数中再次进行了初始化，那么构造函数中的默认值将会覆盖默认的成员初始化值。

```cpp
class Frobber {
 public:
  Frobber() : ptr_(nullptr), length_(0) { }
  Frobber(const char* ptr, size_t length)
    : ptr_(ptr), length_(length) { }
  Frobber(const char* ptr) : ptr_(ptr) { }
 private:
  const char* ptr_;
  // length_ has a non-static class member initializer
  const size_t length_ = strlen(ptr_);
}

```

上面的代码等同于下面这种`C++98`风格的代码:

```cpp
class Frobber {
 public:
  Frobber() : ptr_(nullptr), length_(0) { }
  Frobber(const char* ptr, size_t length)
    : ptr_(ptr), length_(length) { }
  Frobber(const char* ptr)
    : ptr_(ptr), length_(strlen(ptr_)) { }
 private:
  const char* ptr_;
  const size_t length_;
}
```
注意最上面的代码中，`Forbber`的第一个和第二个构造函数对于非静态的数据成员都有进行初始化，这两个构造函数将不会使用给`length_`设置的默认初始化值。然后`Forbber`的第三个构造函数是没有对`length_`进行构造初始化的，因此这个构造函数中`length_`使用的是默认成员初始化的值。

和C++一样，所有的非静态变量都按其声明的顺序进行初始化。

在`Forbber`的三个构造函数中的前两个，构造函数都`length_`成员提供了初始值。那么构造函数的初始化会替代默认的成员初始化，也就是说非静态的类成员初始化将不会为这些构造函数生成初始化的代码。

> 注意，一些来的文档可能将默认成员初始化称为非静态数据成员初始化，简写为NSDMIs.

### Conclusion
默认的成员初始化不会让你的程序变的更快，它只会帮助你减少因为遗漏导致的bug，尤其是当有人添加新的构造函数或新的数据成员时。

主要不要将非静态的类成员初始化与静态的类成员初始化混淆。

```cpp
class Alpha {
 private:
  static int counter_ = 0;
};

```

这是一个老的特性，`counter_`是一个`static`类成员，这是带有初始化值的的静态声明。这与非静态数据成员初始化是不同的。就像静态成员变量与非静态成员变量不同一样。