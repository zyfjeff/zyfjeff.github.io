## Two-Phase Translation

* 模版定义时检查(不检查和模版参数相关的)
  1. 语法错误
  2. 使用了未知的类型、函数名、变量名等
  3. 静态断言

* 模版实例化(和模版参数相关的都要检查)
  1. 所有依赖模版参数的地方都会进行检查

examples:

```
template<typename T>
void foo(T t)
{
     undeclared();        // first-phase compile-time error if undeclared() unknown
     undeclared(t);       // second-phase compile-time error if undeclared(T) unknown
     static_assert(sizeof(int) > 10,          // always fails if sizeof(int)<=10
                   "int too small");
     static_assert(sizeof(T) > 10,            //fails if instantiated for T with size <=10
                   "T too small");
}
```

the type trait std::decay<> is used, which returns the resulting type in a member type
the C++ standard library provides a means to specify choosing “the more general type.”
std::common_type<>::type yields the “common type” of two (or more) different types passed as template arguments.

函数模版、和普通函数可以共同构成函数重载，决策的时候，普通函数有限匹配。除非函数模版可以实例化出最佳匹配。
XXX<> 模版的参数可以是空，表明强制使用函数模版，其模版类型通过参数来推导。

```
template<typename T1, typename T2>
auto max (T1 a, T2 b) {
  return b < a ? a : b;
}

template<typename RT, typename T1, typename T2>
RT max (T1 a, T2 b)
{
  std::cout << "test" << std::endl;
  return b < a ? a : b;
}

int main() {
  auto a = ::max(4, 7.2); // 调用第一个模版
  auto b = ::max<long double>(7.2, 4);  // 调用的是第二个模版
  auto c = ::max<int>(4, 7.2); // 两个都匹配，存在问题
  return 0;
}
```

## 显示初始化的模版，其定义可以不用放在头文件中

```cpp
// print_string.h
template <class T>
void print_string(const T* str);

// print_string.cpp
#include "print_string.h"
template void print_string(const char*);
```

## 函数模版不能部分特化，只能全特化

## FAQ

* 传值还是传引用?
通常意义来说，传引用要比传值效率要高，可以避免不必要的内存拷贝。然后传值通常会更好，出于以下几个原因:

1. 语法更简单
2. 编译时优化更好
3. 移动语义通常比拷贝更好
4. 某些情况是没有拷贝和移动的

此外对于模版来说，还有另外一些原因:
1. 一个模版可以适配简单类型和负责类型，因此选择适合负责类型的传参方式，对于简单类型来说反而适得其反
2. 作为调用者可以仍然可以可以通过`std::ref`和`std::cref`自主选择是传值还是传引用
3. 传递常量字符串会成为大问题，但是通过引用来传递被认为会造成更大的问题

* 为什么不`inline`?
通常来说函数模版不需要声明为inline，因为我们可以定义noinline的函数模版在一个头文件中，然后可以在多个编译单元中包含这个头文件。
inline本身的目的就是在多个地方进行展开，避免函数调用的进栈和入栈的开销，但是对于全特化的模版来说不适用这个规则。

* 为什么不`constexpr`?
TODO

