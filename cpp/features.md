# C++17

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