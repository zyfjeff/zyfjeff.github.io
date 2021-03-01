---
hide:
  - toc        # Hide table of contents
---
# C++基础
## shared_ptr实现copy-on-write

1. read端加锁copy，返回访问拷贝的shared_ptr
2. write端加锁判断是否是unique，如果是unique直接原地修改，否则拷贝一份并将shared_ptr指向新的位置。

## Dependent base

https://gcc.gnu.org/wiki/VerboseDiagnostics#dependent_base

## `this->template`

template是用来消除歧义的. 观察下面的代码:

```cpp
template<class T>
 int f(T& x) {
     return x.template convert<3>(pi);
 }
```

如果没有template, 则`return x.convert<3>(pi);` 可能被理解为`return ((x.convert) < 3) > (pi)`


## iterator traits

```cpp

// 迭代器的类型
namespace std {
  struct output_iterator_tag {};
  struct input_iterator_tag {};
  struct forward_iterator_tag : public input_iterator_tag {};
  struct bidirectional_iterator_tag : public forward_iterator_tag {};
  struct random_access_iterator_tag : public bidirectional_iterator_tag {};
}

// 所有迭代器的公共类型信息
namespace std {
template <typename T>
struct iterator_traits {
    typedef typename T::iterator_category   iterator_category;
    typedef typename T::value_type          value_type;
    typedef typename T::difference_type     difference_type;
    typedef typename T::pointer             pointer;
    typedef typename T::reference           reference;
};
}
```

`std::iterator_traits<T>::value_type val;` 通过这种方式来使用迭代器。

迭代器分类的来实现优化策略:

```cpp
template <typename Iterator>
void f(Iterator beg, Iterator end) {
    f(beg, end, std::iterator_traits<Iterator>::iterator_category());
}

// special f for random-access iterators.
template <typename RandomIterator>
void f(RandomIterator beg, RandomIterator end, std::random_access_iterator_tag) {
    //...
    // 可以随机访问
}

// special f for bidirectional terators.
template <typename BidirectionalIterator>
void f(BidirectionalIterator beg, BidirectionalIterator end, std::bidirectional_iterator_tag) {
    // ...
    // 可以双向访问
}
```

## 底层、顶层const

* 如果const右结合修饰的为类型或者*，那这个const就是一个底层const
* 如果const右结合修饰的为标识符，那这个const就是一个顶层const


哪些是底层const哪些是顶层const?

* int，double，float和long long等基本内置数据类型的const都是顶层const。
* 引用的const都是底层const。
* 指针既可以是顶层const也可以是底层const，也可以同时是两种const。


```cpp
int const * const p;
      ^       ^
      1       2
```

1. 底层const(上述代码中1的位置)主要影响的是指向的对象，表示指向的对象不能改变其内容，但是可以改变p本身的指向，使它指向另外一个对象
2. 顶层const(上述代码中2的位置)主要影响的是对象本身，表示对象p本身无法修改，也就是没办法指向另外一个对象


* 底层const是不可忽略的。

```cpp
const char* str = "996ICU251";
char *c = str;//不合法！
```

> 当执行对象的拷贝过程中（赋值操作，函数的值传递）时，如果被拷贝对象拥有底层const资格，则拷贝对象必须拥有相同的底层const资格。
> 如果拷贝对象拥有底层const，则无所谓被拷贝对象是否有const。
> 对于一个既是顶层const又是底层const对象来说，无所谓它是否为顶层const，只要关注它的底层const就行了

## Literal Type

C++ core language对其进行了定义，一个LiteralType类型满足下面的要求:

1. 可以带有cv限制符的void
2. 基本类型(scalar type)
3. 引用类型
4. lister type的数组
5. 可以带有cv限制符的class类型，但是需要同时满足下面需求:
  1. 有trivial destructor
  2. 下面任何一个需求
    1. Aggregate Type
    2. 类型中至少有一个constexpr构造函数，并且不是copy或者move构造
    3. 一个closure type
  3. 如果是unions类型，那么至少有一个非static的数据成员是 non-volatile literal type
  4. 如果不是unions类型，那么所有的非static数据成员，以及基类都需要是 non-volatile literal type
  5. 所有的非static数据成员，以及基类都需要是 non-volatile literal type

Only literal types may be used as parameters to or returned from constexpr functions. Only literal classes may have constexpr member functions.

只有listeral type才可以作为constexpr函数的参数和返回值。只有literal type的类才可能有constexpr成员函数

## Aggregate Type

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

## `std::move_iterator`

```cpp
#include <iostream>
#include <algorithm>
#include <vector>
#include <iterator>
#include <numeric>
#include <string>

int main()
{
    std::vector<std::string> v{"this", "is", "an", "example"};

    std::cout << "Old contents of the vector: ";
    for (auto& s : v)
        std::cout << '"' << s << "\" ";

    typedef std::vector<std::string>::iterator iter_t;
    std::string concat = std::accumulate(
                             std::move_iterator<iter_t>(v.begin()),
                             std::move_iterator<iter_t>(v.end()),
                             std::string());  // Can be simplified with std::make_move_iterator

    std::cout << "\nConcatenated as string: " << concat << '\n'
              << "New contents of the vector: ";
    for (auto& s : v)
        std::cout << '"' << s << "\" ";
    std::cout << '\n';
}
```

还可以借助`std::make_move_iterator`，可以不用显示给出iterator类型

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


## function And Move Lambda

当一个lambda函数使用move捕获了一个只能move的变量，那么这个lambda将无法复制和移动给一个function，需要使用模版来解决

```c++
class HasCallback
{
  public:
    // 改成void setCallback(T&& f)即可解决

    void setCallback(std::function<void(void)>&& f)
    {
      callback = move(f);
    }

    std::function<void(void)> callback;
};

int main()
{
  auto uniq = make_unique<std::string>("Blah blah blah");
  HasCallback hc;
  hc.setCallback(
      [uniq = move(uniq)](void)
      {
        std::cout << *uniq << std::endl;
      });

  hc.callback();
}
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


## 委托模式(Delegate)

```C++
class Consumer {
  public:
    class Delegate {
      public:
        virtual ~Delegate() {}
        virtual void Unregister(Consumer* consumer) PURE;
    };
  Consumer(Delegate *delegate);
  DoUnregister() {
    if (delegate_) {
      delegate_->Unregister();
    }
  }
  private:
    Delegate* delegate_;
}
```

相当于Consumer将部分能力通过Delegate的方式提供出去，让别的组件来实现，然后自己委托来调用。这个实现存在的问题就是
Delagate的生命周期并不是由Consumer来保证的，如何确保在调用的时候，Delegate是存活的?


## Type erasure
在一些动态语言中，一个变量可以是任意类型，我们称之为duct type，



## 自定义迭代器

C++中将迭代器分为六类，分别是:

1. Input Iterator 只能顺序扫描一次，并且是只读的
2. Output Iterator  只能顺序扫描一次，并且不能读，只能写
3. Forward Iterator 可以顺序扫描多次，并且可读可写
4. Bidirectional Iterator 可以双向扫描，并且可读可写
5. Random Access Iterator 可以双向扫描、并且还支持随机访问，可读可写(逻辑上要求元素是相邻的)
6. Contiguous Iterator  具备上升迭代器所有的特点，此外还要求内部元素在物理上是相连的(比如std::array，std::deque不具备这个特点)

> 通过迭代器分类可以辅助算法进行性能优化，以及对输入进行校验，比如std::fill要求迭代器是Forward Iterator，如果这个时候输入的迭代器是Input Iterator/ Output Iterator


C++17之前实现自定义迭代器都是通过`tag dispatch`的方式来实现，Forward Iterator example如下：

```C++
#include <iterator> // For std::forward_iterator_tag
#include <cstddef>  // For std::ptrdiff_t

class Integers {
public:
  struct Iterator {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = int;
    using pointer = int*;
    using reference = int&;

    Iterator(pointer ptr) : ptr_(ptr) {}
    reference operator*() { return *ptr_; }
    pointer operator->() { return ptr_; }

    Iterator& operator++() {
      ptr_++;
      return *this;
    }

    Iterator operator++(int) {
      Iterator tmp = *this; ++(*this); return tmp;
    }

    friend bool operator==(const Iterator& a, const Iterator& b) {
      return a.ptr_ == b.ptr_;
    }

    friend bool operator!=(const Iterator& a, const Iterator& b) {
      return a.ptr_ == b.ptr_;
    }

    Iterator begin() { return Iterator(&data_[0]); }
    Iterator end()   { return Iterator(&data_[200]); } // 200 is out of bounds

    private:
      pointer ptr_;
  };
private:
  int data_[200];
};


```

一个典型的迭代器预期要有下面几个属性:

1. `iterator_category`  迭代器所属于的类型，上文中提到的六类迭代器
2. `difference_type` 用于标识迭代器步长，通常我们的迭代器都是指针，因此选用`std::ptrdiff_t`就好了。
3. `value_type` 迭代器要迭代的类型，例如上面的这个例子中我们的类型是`int`
4. `pointer`  定义一个指向迭代类型的指针，上面的例子中是`int*`
5. `reference` 定义一个迭代类型的引用，上面的例子中是`int&`


> 迭代器本身必须是可构造、Copy构造、Copy赋值、析构、和可交换的。

对于一个Forward Iterator来说，最基本要实现`*iterator`、`iterator->x`、`++iterator`、`iterator++`、`iterator_a == iterator_b` 这几类操作


Ref: https://www.internalpointers.com/post/writing-custom-iterators-modern-cpp



## 为什么模版的声明和定义要放在一起?

## 为什么make_shared无法自定义删除器

## unique_ptr为什么不能和shared_ptr一样不把删除器保存在模版参数中

## shared_ptr自定义的删除器保存在哪，为什么可以不用放在模版参数中

## 常用链接

1. https://godbolt.org/
2. https://preshing.com/