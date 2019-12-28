# C++17

## Stricter Expression Evaluation Order

```C++
#include <iostream>

class Query {
 public:
  Query& addInt(int i) {
    std::cout << "addInt: " << i << '\n';
    return *this;
  }

  Query& addFloat(float f) {
    std::cout << "addFloat: " << f << '\n';
    return *this;
  }
};

float comuteFloat() {
  std::cout << "computing float... \n";
  return 10.1f;
}

float computeInt() {
  std::cout << "computing int... \n";
  return 8;
}

int main() {
  Query q;
  q.addFloat(comuteFloat()).addInt(computeInt());
}
```

在C++17之前，输出的顺序可能如下(从右向左先计算参数的值，然后再开始执行):

```C++
computing int...
computing float...
addFloat: 10.1
addInt: 8
```

在C++17后，输出的顺序可以保证按照代码上来顺序来执行。

```C++
computing float...
addFloat: 10.1
computing int...
addInt: 8
```


`C++17`之前对于形如`A(B(), C());`这种形式的调用，对于`B()`和`C()`的执行顺序是不确定的，所以为了保证内存安全，才有了`std::make_unique`这样的语法糖。

例如下面这段代码，如果先执行`new T`，然后执行`otherFunction`，这个时候如果抛出异常会导致内存泄漏，但是C++17后，可以保证new T执行完后，再执行`unique_ptr<T>`的构造，
最后才执行`otherFunction`，这样就可以保证内存安全了。

```C++
foo(unique_ptr<T>(new T), otherFunction());
```

C++17后对于表达式的执行顺序可以进行如下的保障。

  1. a.b
  2. a->b
  3. a->*b
  4. a(b1, b2, b3) // b1, b2, b3 - in any order 5. b @= a // '@' means any operator
  6. a[b]
  7. a << b
  8. a >> b


## Guaranteed Copy Elision

C++17之前要求实现RVO的对象必须要有Copy构造或者移动构造函数。例如下面这段代码将无法进行编译，这是因为C++17之前可能会退化成拷贝/移动构造，所以要求
实现RVO的对象需要具备拷贝/移动构造函数。

```C++
#include <array>

// Chapter Clarification/copy_elision_non_moveable.cpp #include <array>
// based on P0135R0
struct NonMoveable {
  NonMoveable(int x) : v(x) { }
  NonMoveable(const NonMoveable&) = delete;
  NonMoveable(NonMoveable&&) = delete;
  std::array<int, 1024> arr;
  int v;
};

NonMoveable make(int val) {
        if (val > 0)
                return NonMoveable(val);
        return NonMoveable(-val);
}

int main() {
  auto largeNonMoveableObj = make(90);
  return largeNonMoveableObj.v;
}


// 编译错误

copy_elision_non_moveable.cc:15:10: error: call to deleted constructor of 'NonMoveable'
                return NonMoveable(val);
                       ^~~~~~~~~~~~~~~~
copy_elision_non_moveable.cc:8:3: note: 'NonMoveable' has been explicitly marked deleted here
  NonMoveable(NonMoveable&&) = delete;
  ^
copy_elision_non_moveable.cc:16:9: error: call to deleted constructor of 'NonMoveable'
        return NonMoveable(-val);
               ^~~~~~~~~~~~~~~~~
copy_elision_non_moveable.cc:8:3: note: 'NonMoveable' has been explicitly marked deleted here
  NonMoveable(NonMoveable&&) = delete;
  ^
copy_elision_non_moveable.cc:20:8: error: call to deleted constructor of 'NonMoveable'
  auto largeNonMoveableObj = make(90);
       ^                     ~~~~~~~~
copy_elision_non_moveable.cc:8:3: note: 'NonMoveable' has been explicitly marked deleted here
  NonMoveable(NonMoveable&&) = delete;
```

C++17后上面的代码就可以编译了，会保证强制执行执行RVO优化(两种case下会保证)，所以可以直接原地构造。但是这一切的保证只适用于RVO，而不是NRVO。

* 形如`Type t = T()`从一个右值来初始化一个对象
* 函数返回的一个右值来初始化对象`Type t = functioncall()`


## Updated Value Categories

C++98/03的时候对于值的类型分为两类:

* lvaluue
* rvalue

C++11后，则分为五类:

* lvalue
* glvalue
* xvalue  通过右值引用产生的右值，其实又是左值

```C++
void process_value(int& i) {
 std::cout << "LValue processed: " << i << std::endl;
}
void process_value(int&& i) {
 std::cout << "RValue processed: " << i << std::endl;
}
void forward_value(int&& i) {
 process_value(i);
}

int main() {
 int a = 0;
 process_value(a);
 process_value(1);
 forward_value(2);
}

// 输出结果:
LValue processed: 0
RValue processed: 1
LValue processed: 2
```

* rvalue
* prvalue


xvalue和lvalue 属于 glvalue
prvalue和xvalue 属于 rvalue

## Dynamic Memory Allocation for Over-Aligned Data

C++17后给new/delete添加了内存对其的参数，使其可以分配内存对齐的地址，C++17之前只能在定义的时候声明其内存对齐的大小
但是不能保证分配出的内存是对齐的。

```C++
// 声明的时候指定了内存对齐的大小，但是实际内存分配的时候，并不一定是对齐的。
class alignas(16) vec4 {
  float x, y, z, w;
};
operator new[](sizeof(vec4))
```

C++17后可以new会自动进行对齐

```C++
operator new[](sizeof(vec4), align_val_t(alignof(vec4))
```

## Exception Specifications as Part of the Type System

C++17后异常信息是类型系统的一部分。

```C++
#include <iostream>
int main() {
  void (*p)();
  void(**pp)() noexcept = &p;
}

error: cannot initialize a variable of type 'void (**)() noexcept' with an rvalue of type 'void (**)()'
```

## Structured Binding Declarations

语法格式如下：

```C++
auto [a, b, c, ...] = expression;
auto [a, b, c, ...] = { expression };
auto [a, b, c, ...] = ( expression );
```

一个例子如下:

```C++
#include <iostream>

std::pair<int, bool> InsertElement(int el) {
  return std::make_pair(10, false);
}
int main() {
  int index{ 0 };
  bool flag{ false };

  // C++17之前需要通过std::tie来解开pair
  std::tie(index, flag) = InsertElement(10);

  // C++17后可以直接进行结构化绑定
  auto [index_new, flag_new] = InsertElement(10);
  std::cout << index_new << ":" <<  flag_new << std::endl;
  return 0;
}
```

默认结构化绑定是copy的方式，也可以使用引用、右值引用、const等方式

```C++
const auto [a, b, c, ...] = expression
auto& [a, b, c, ...] = expression
auto&& [a, b, c, ...] = expression
```

```C++
  std::pair b(std::string("abcd"), 1.0f);
  auto&& [c, d] = b;
  std::cout << c << std::endl;
  // output empty string
  std::cout << a.first << std::endl;
```

结构化绑定除了可以应用在`std::pair`上(准确的说是支持std::tuple_size<>和提供了get<N>方法的类型)，还可以应用到数组、非static的公共成员等

```C++
  double myArray[3] = {1.0, 2.0, 3.0};
  auto [e, f, g] = myArray;
  std::cout << e << f << g << std::endl;
```



## Template argument deduction for class templates

自动推到类模版的参数、不用显示的指定模版类型

```cpp
template <typename T = float>
struct MyContainer {
  T val;
  MyContainer() : val() {}
  MyContainer(T val) : val(val) {}
  // ...
};
MyContainer c1 {1}; // OK MyContainer<int>
MyContainer c2; // OK MyContainer<float>
```

## 折叠表达式

* 一元右折叠 `(E op ...) 成为 (E1 op (... op (EN-1 op EN)))`
* 一元左折叠 `(... op E) 成为 (((E1 op E2) op ...) op EN)`
* 二元右折叠 `(E op ... op I) 成为 (E1 op (... op (EN−1 op (EN op I))))`
* 二元左折叠 `(I op ... op E) 成为 ((((I op E1) op E2) op ...) op EN)`

## 列表初始化自动推导

```cpp
auto x1 {1, 2, 3}; // error: not a single element
auto x2 = {1, 2, 3}; // decltype(x2) is std::initializer_list<int>
auto x3 {3}; // decltype(x3) is int
auto x4 {3.0}; // decltype(x4) is double
```

## Lambda通过值语义拷贝this

这个在C++17之前是做不到的，只能传引用，或者指针拷贝

```cpp
struct MyObj {
  int value {123};
  // this 指针拷贝
  auto getValueCopy() {
    return [*this] { return value; };
  }
  auto getValueRef() {
    return [this] { return value; };
  }
};
MyObj mo;
auto valueCopy = mo.getValueCopy();
auto valueRef = mo.getValueRef();
mo.value = 321;
valueCopy(); // 123
valueRef(); // 321
```

## constexpr if


```cpp
#include <type_traits>

template <typename T>
constexpr bool isIntegral() {
  if constexpr (std::is_integral<T>::value) {
          return true;
  } else {
          return false;
  }
}

int main() {
  static_assert(isIntegral<int>() == true);
  static_assert(isIntegral<char>() == true);
  static_assert(isIntegral<double>() == false);
  struct S {};
  static_assert(isIntegral<S>() == false);
  return 0;
}
```

## Direct List Initialization of Enums

```cpp
enum byte : unsigned char {};
byte b {0}; // OK
byte c {-1}; // ERROR
byte d = byte{1}; // OK
byte e = byte{256}; // ERROR
```

## std::variant

```cpp
std::variant<int, double> v {12};
std::get<int>(v); // == 12
std::get<0>(v); // == 12
v = 12.0;
std::get<double>(v); // == 12.0
std::get<1>(v); // == 12.0
```

## std::optional

```cpp
std::optional<std::string> create(bool b) {
  if (b) {
    return "Godzilla";
  } else {
    return {};
  }
}

create(false).value_or("empty"); // == "empty"
create(true).value(); // == "Godzilla"
// optional-returning factory functions are usable as conditions of while and if
if (auto str = create(true)) {
  // ...
}
```

## std::any

```cpp
std::any x {5};
x.has_value() // == true
std::any_cast<int>(x) // == 5
std::any_cast<int&>(x) = 10;
std::any_cast<int>(x) // == 10
```

## std::string_view

```cpp
// Regular strings.
std::string_view cppstr {"foo"};
// Wide strings.
std::wstring_view wcstr_v {L"baz"};
// Character arrays.
char array[3] = {'b', 'a', 'r'};
std::string_view array_v(array, std::size(array));
```

## std::invoke

定规如下:

```cpp
template< class F, class... Args>
std::invoke_result_t<F, Args...> invoke(F&& f, Args&&... args) noexcept(/* see below */);
```

参数当作可变模版参数来传递

```cpp
template <typename Callable>
class Proxy {
    Callable c;
public:
    Proxy(Callable c): c(c) {}
    template <class... Args>
    decltype(auto) operator()(Args&&... args) {
        // ...
        return std::invoke(c, std::forward<Args>(args)...);
    }
};
auto add = [](int x, int y) {
  return x + y;
};
Proxy<decltype(add)> p {add};
p(1, 2); // == 3
```

## std::apply

其定义如下:

```cpp
template <class F, class Tuple>
constexpr decltype(auto) apply(F&& f, Tuple&& t);
```

参数当作tuple来传递

```cpp
auto add = [](int x, int y) {
  return x + y;
};
std::apply(add, std::make_tuple(1, 2)); // == 3
```

## Splicing for maps and sets

通过extract可以在两个map之间来回移动元素，没有任何拷贝成本

```cpp
std::map<int, string> src {{1, "one"}, {2, "two"}, {3, "buckle my shoe"}};
std::map<int, string> dst {{3, "three"}};
dst.insert(src.extract(src.find(1))); // Cheap remove and insert of { 1, "one" } from `src` to `dst`.
dst.insert(src.extract(2)); // Cheap remove and insert of { 2, "two" } from `src` to `dst`.
// dst == { { 1, "one" }, { 2, "two" }, { 3, "three" } };
```

## std::filesystem

## std::byte

## 并行算法

# C++14

## Lambda capture initializers

通过捕获表达式初始化可以

```cpp
int factory(int i) { return i * 10; }
auto f = [x = factory(2)] { return x; }; // returns 20

auto generator = [x = 0] () mutable {
  // this would not compile without 'mutable' as we are modifying x on each call
  return x++;
};
auto a = generator(); // == 0
auto b = generator(); // == 1
auto c = generator(); // == 2
```

借助捕获表达式初始化来完成变量move到lambda中

```cpp
auto p = std::make_unique<int>(1);

// 没办法拷贝
auto task1 = [=] { *p = 5; }; // ERROR: std::unique_ptr cannot be copied
// vs.
// move进去
auto task2 = [p = std::move(p)] { *p = 5; }; // OK: p is move-constructed into the closure object
// the original p is empty after task2 is created
```


## decltype(auto)

直接使用auto的话会忽略调cv限制符

```cpp
const int x = 0;
auto x1 = x; // int
decltype(auto) x2 = x; // const int
int y = 0;
int& y1 = y;
auto y2 = y1; // int
decltype(auto) y3 = y1; // int&
int&& z = 0;
auto z1 = std::move(z); // int
decltype(auto) z2 = std::move(z); // int&&

```