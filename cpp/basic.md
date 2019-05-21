# 编程实践

## static and thread local Initialization
这两类变量被称为Non-local variables，static变量会在main函数启动之前进行初始化(除非主动deferred)，所有的thread_local
变量则会在线程运行之前进行初始化。

> 在函数内声明的static变量属于 local static

初始化又分为两种:

* 静态初始化

  1. `Constant initialization` 理论上应该在编译期完成，预先计算好值，即使编译器不这样做，也要保证这类初始化在动态初始化之前先完成
  2. `Zero initialization` 没有提供任何初始化值的，non-local static和thread local变量的初始化，会放在.bss段，不占用磁盘空间。

* 动态初始化

  1. `Unordered dynamic initialization` static/thread-local类模版、static 数据成员、可变模版等
  2. `Partially-ordered dynamic initialization` (C++17)
  3. `Ordered dynamic initialization` 相同编译单元non-local variables是顺序的。

* constant initialization

```
static T & ref = constexpr;
static T object = constexpr;
```

* zero initialization

```
static T object ;
CharT array [ n ] = "";
T () ;
T t = {} ;
T {} ;
```

## 使用duration_cast使得时间转化的代码可读性更高

```cpp
std::chrono::microseconds us = std::chrono::duration_cast<std::chrono::microseconds>(d);
timeval tv;
tv.tv_sec = us.count() / 1000000; # 这种操作易读性不高
```

换成下面这种形式

```cpp
auto secs = std::chrono::duration_cast<std::chrono::seconds>(d);
auto usecs = std::chrono::duration_cast<std::chrono::microseconds>(d - secs);
tv.tv_secs = secs.count();
tv.tv_usecs = usecs.count();
```

## 使用Gdb来dump内存信息

```
sudo gdb
> attach 进程PID
> i proc mappings  # 查看进程的地址空间信息 等同于 /proc/PID/maps
> i files  # 可以看到更为详细的内存信息 等同于 /proc/PID/smaps
> dump memory 要存放的位置 SRC DST # 把SRC到DST 地址范围内的内存信息dump出来

对dump出来的内容实用strings即可查看到该段内存中存放的字符串信息了
```

## 引用折叠和cv限制符

引用折叠规则:
1. 所有右值引用折叠到右值引用上仍然是一个右值引用
2. 所有的其他引用类型之间的折叠都将变成左值引用

但是当发生引用折叠的时候，cv限制符会被去掉，例如下面这个例子:

```cpp
template<typename T>
void refFold(const T& data) {}

int c = 0;
refFold<int&>(c);
refFold<int&>(0); # compile error
```

## 强类型

参考文章:
1. https://www.fluentcpp.com/2016/12/05/named-constructors/
2. https://foonathan.net/blog/2016/10/19/strong-typedefs.html


## 转发引用
所谓转发，就是通过一个函数将参数继续转交给另一个函数进行处理，原参数可能是右值，可能是左值，如果还能继续保持参数的原有特征，那么它就是完美的。
什么是原有特征呢? C++主要有两类，一类就是CV限制符号(const/non const)，另外一类就是左值/右值，完美转发指的就是在参数传递过程中，这两组属性不变。
说到转发引用，有两个术语来表示，第一个是forwarding reference，另外一个则是univeral refernence，需要配合模版一起才能被称之为转发引用，其形式如下:

```
template <typename T>
void f(T &&t) {
  g(std::forward<T>(t));
}

template <typename... Args>
void f(Args&&... args) {
  g(std::forward<Args>(args)...);
}
```
其他任何形式都不能称之为转发引用，即使只是添加了一个CV限制符。

## 函数模版无法偏特化，只能全特化

## extern template


## 模版模版参数

```cpp
#include <vector>
#include <iostream>

template <class T, template <class...> class C, class U>
C<T> cast_all(const C<U> &c) {
        C<T> result(c.begin(), c.end());
  return result;
}

int main() {
        std::vector<float> vf = {1.2, 2.6, 3.78};
        // 传入一个模版类型，自动推导出模版的类型。
        auto vi = cast_all<int>(vf);

        for (auto &&i : vi) {
                std::cout << i << std::endl;
        }

        return 0;
}
```

## 可变参数模版
Start with the general (empty) definition, which also serves as the base-case for recrusion termination in the later specialisation:

```cpp
template<typename ... T>
struct DataStructure {};

template<typename T, typename ... Reset>
struct DataStructure<T, Reset ...> {
  DataStructure(const T& first, const Reset& ... reset)
    : first(first)
    , reset(reset...) {}
  T first;
  DataStructure<Reset ... > reset;
};

// 解开的过程大致如下:
DataStructure<int, float>
   -> int first
   -> DataStructure<float> rest
         -> float first
         -> DataStructure<> rest
            -> (empty)

对于如下这样的模版，想要获取第三个元素则通过data.reset.reset.first来获取
DataStructure<int, float, std::string>

为了简化上述的获取过程，可以写一些helper函数来辅助完成

template<typename T, typename ... Rest>
struct DataStructure<T, Rest ...> {
  ...
  template<size_t idx>
  auto get() {
      return GetHelper<idx, DataStructure<T,Rest...>>::get(*this);
  }
  ...
};

template<typename T, typename ... Rest>
struct GetHelper<0, DataStructure<T, Rest ... >> {
  static T get(DataStructure<T, Rest...>& data) {
      return data.first;
  }
};

template<size_t idx, typename T, typename ... Rest>
struct GetHelper<idx, DataStructure<T, Rest ... >> {
  static auto get(DataStructure<T, Rest...>& data) {
    return GetHelper<idx-1, DataStructure<Rest ...>>::get(data.rest);
  }
};

```

C++17中，可以简化可变模版参数展开的问题

```cpp
template <typename... Ts>
void print_all(std::ostream& os, Ts const&... args) {
  ((os << args), ...);
}

template <typename T, class... Ts>
void print_all(std::ostream& os, T const& first, Ts const&... rest) {
  os << first;
  if constexpr (sizeof...(rest) > 0) {
    print_all(os, rest...);
  }
}

```

Ref: `code/variadic_template/variadic.cc`

## Iterators

通过rbegin、rend返回的是反向迭代器reverse_iterator，通过其base方法可以转换为正向迭代器

```cpp
std::vector<int>::reverse_iterator r = v.rbegin();
std::vector<int>::iterator i = r.base();
assert(&*r == &*(i - 1));
```

std::istream_iterator 输入迭代器
std::ostream_iterator 输出迭代器

```cpp
// 会忽略空白字符
std::istringstream istr("1\t 2   3 4");
std::vector<int> v;

std::copy(
  std::istream_iterator<int>(istr),
  std::istream_iterator<int>(),
  std::back_inserter(v));

std::copy(v.begin(), v.end(),
  std::ostream_iterator<int>(std::cout, " -- "));
```

自定义迭代器


## type traits

* `std::conditional` 类似于三目运算符，根据编译期条件来选择

```cpp
template<typename T>
struct ValueOfPointer {
  typename std::conditional<(sizeof(T) > sizeof(void*), T*, T>::type vop;
}
```

* `std::common_type` 获取多个类型之间都可以隐式转换到的共同类型

```cpp
// 通过std::common_type获取T1和T2的共同类型作为返回值的类型
template <typename T1, typename T2>
auto min(const T1 &a, const T2 &b) -> typename std::common_type<const T1&, const T2&>::type {
  return a < b ? a : b;
}
```

* `std::declval` 不论类型的构造函数如何都可以通过这个traits获取到这个类型的一个对象的引用实例，常和`decltype`结合使用

```cpp
#include <utility>
#include <iostream>

struct Default { int foo() const { return 1; } };

struct NonDefault
{
    NonDefault(const NonDefault&) { }
    int foo() const { return 1; }
};

int main()
{
    decltype(Default().foo()) n1 = 1;                   // type of n1 is int
//  decltype(NonDefault().foo()) n2 = n1;               // error: no default constructor
    decltype(std::declval<NonDefault>().foo()) n2 = n1; // type of n2 is int
    std::cout << "n1 = " << n1 << '\n'
              << "n2 = " << n2 << '\n';
}
```

* `std::iterator_traits`获取迭代器的分类，可以知道这个迭代器的值类型、是哪个类别等信息

```cpp
template < typename BidirIt>
void test(BidirIt a, std::bidirectional_iterator_tag) {
  std::cout << "bidirectional_iterator_tag is used" << std::endl;
}

template < typename ForwIt>
void test(ForwIt a, std::forward_iterator_tag) {
  std::cout << "Forward iterator is used" << std::endl;
}

template < typename Iter>
void test(Iter a) {
  test(a, std::iterator_traits<Iter>::iterator_category());
}
```

## 在析构函数中调用const成员函数

const and volatile semantics (7.1.6.1) are not applied on an object under destruction.
They stop being in effect when the destructor for the most derived object (1.8) starts.

```
class Stuff
{
public:
    // const、volatile语义在析构的时候是无用的，所以这里调用的foo()总是 non-const的。
    ~Stuff() { foo(); }

    void foo() const { cout << "const foo" << endl; }
    void foo()       { cout << "non-const foo" << endl; }
};
```

Ref: https://stackoverflow.com/questions/53840945/figuring-out-the-constness-of-an-object-within-its-destructor

## constexpr 和 static_assert

```
#include <iostream>
#include <type_traits>

template<class T> struct dependent_false : std::false_type {};

template <typename T>
void f() {
     if constexpr (std::is_arithmetic<T>::value) {
     } else {
       // 这里不能直接 static_assert(false, "Must be aruthmetic");
       static_assert(dependent_false<T>::value, "Must be arithmetic"); // ok
     }
}

int main() {
  f<int*>();
}
```
Ref: https://en.cppreference.com/w/cpp/language/if#Constexpr_If

## Curiously Recurring Template Pattern (CRTP)
递归的奇异模版，有两个特点:
1. 从模版类继承
2. 使用派生类自己作为基类的模版参数

```cpp
template <typename T>
class Base {};

class Dervived : public Base<Dervived> {};
```

这样做的目的就是为了能在基类中使用派生类，这样基类就可以通过`static_cast`来使用派生类。

```cpp
template <typename T>
class Base {
 public:
  void doSomething() {
    T& derived = static_cast<T&>(*this);
  }
};
```
但是这存在一个问题，如果基类模版参数弄错了怎么办?，例如下面这个例子:

```cpp
class Derived1 : public Base<Derived1> {};
class Derived2 : public Base<Derived1> {};// bug in this line of code
```

`Marek Kurdej`提出了一个解决方案，在基类中将对应的派生类设置为friend;

```cpp
template <typename T>
class Base {
public:
private:
    Base(){};
    friend T;
};
```
这样的化如果模版类的参数是错误的就会导致基类构造失败，因为基类的构造函数是私有的，只有其友元函数可以派生自基类

CRTP的用处很大，这里列举两个场景:

1. 给派生类添加功能

```cpp
template <typename T>
struct AddOperator {
  T operator+ (const T& t) const {
    return T(t.value() + value());
  }

  int value() const { return static_cast<const T*>(this)->value(); }
};

class test : public AddOperator<test> {
 public:
  test(int value) : value_(value) {}
  int value() const { return value_; }
 private:
  int value_;
};

int main() {
  test t1(11), t2(20);
  std::cout << (t1 + t2).value() << std::endl;
  return 0;
}
```

2. 静态多态

```cpp
template <typename T>
class Amount {
 public:
  double getValue() const {
    return static_cast<T const&>(*this).getValue();
  }
};

class Constant42 : public Amount<Constant42> {
 public:
  double getValue() const { return 42; }
};

class Variable : public Amount<Variable> {
 public:
  explicit Variable(int value) : value_(value) {}
  double getValue() const { return value_; }
 private:
  int value_;
};

template<typename T>
void print(Amount<T> const& amount) {
  std::cout << amount.getValue() << "\n";
}

Constant42 c42;
print(c42);
Variable v(43);
print(v);
```

上面的代码在基类中调用派生类的特定方法(`getValue`)，所有继承自这个基类的派生类都包含了这个方法。
但是这些派生类的父类不相同，不能像正常的多台方式(基类指针指向派生类)来调用，但是它们的基类都是`Amount<T>`，
这样就可以通过模版的方式来调用这些派生类。上面的print函数模版就是一个典型的例子。

仔细观察Amount模版类可以发现有一个共同点就是会通过static_cast来将基类转化为派生类，这个可以抽离成一个公共模块。

```cpp
template <typename T>
struct crtp
{
    T& underlying() { return static_cast<T&>(*this); }
    T const& underlying() const { return static_cast<T const&>(*this); }
};

template<typename T>
class Amount : public crtp<T> {
  double getValue() const {
    return this->underlaying().getValue();
  }
}
```

但是上面会遇到菱形继承的问题

```cpp
template <typename T>
struct Scale : crtp<T> {
 void scale(double multiplicator) {
   this->underlying().setValue(this->underlying().getValue() * multiplicator);
 }
};

template <typename T>
struct Square : crtp<T> {
 void square() {
   this->underlying().setValue(this->underlying().getValue() * this->underlying().getValue());
 }
};

class Sensitivity : public Scale<Sensitivity>, public Square<Sensitivity> {
 public:
  double getValue() const { return value_; }
  void setValue(double value) { value_ = value; }

private:
  double value_;
};
```

上面的代码中最终的基类都是`crtp<Sensitivity>`,为了解决这个问题可以给crtp添加一个额外的模版参数来避免

```cpp
template <typename T, template<typename> class crtpType>
struct crtp {
 public:
  T& underlying() { return static_cast<T&>(*this); }
  const T& underlying() const { return static_cast<const T&>(*this); }
 private:
  crtp() {}
  friend crtpType<T>;
};
```


## Mock技巧

1. Mock `ClassA::Create()`


``` cpp

class AInterface;

class A : public A::AInterface {
 public:
  static std::unique_ptr<A::AInterface> Create();
};

class B {
 public:
  void Method() {
    a_ = A::Create();
  }
 private:
  std::unique_ptr<A::AInterface> a_;
}
```

上面的类A通过静态方法创建实例，现在想要测试B，需要把A Mock掉。可以通过下面这种方式来Mock

```cpp
class B {
 public:
  void Method() {
    a_ = Create();
  }
  virtual std::unique_ptr<A::AInterface> Create();
 private:
  std::unique_ptr<A::AInterface> a_;
};

class ProdB : public B {
 public:
  virtual std::unique_ptr<A::AInterface> Create() override {
    return A::Create();
  }
};
```

## 线程/进程模型总结

## 网络模型总结