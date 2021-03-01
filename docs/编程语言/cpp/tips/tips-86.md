---
hide:
  - toc        # Hide table of contents
---
# Tip of the Week #86: Enumerating with Class

> Originally posted as totw/86 on 2015-01-05
> By Bradley White (bww@google.com)
> “Show class, … and display character.” - Bear Bryant.


枚举，或者简单地说是`enum`，是一种可以保存指定整数集之一的类型。这个集合的一些值可以被命名为枚举值。


## Unscoped Enumerations

这个对于C++程序员来说不会陌生，但是在C++11之前枚举有两个缺点，枚举的名称:

* 和枚举类型相同的scope内
* 枚举的值可以隐式转换为整型

因此在C++98时，可以像下面这样:

```cpp
enum CursorDirection { kLeft, kRight, kUp, kDown };
CursorDirection d = kLeft; // OK: enumerator in scope
int i = kRight;            // OK: enumerator converts to int
```

但是这会导致枚举值的重定义，下面是一个新的枚举类型，但是枚举值却发生了重定义

```cpp
// error: redeclarations of kLeft and kRight
enum PoliticalOrientation { kLeft, kCenter, kRight };
```

`C++11`以一种新的方式修改了未作用域枚举的行为：枚举值要通过枚举类型来引用，但是为了兼容，仍然支持直接使用枚举值，已经隐式转换为整型。

因此`C++11`后，可以这样来使用枚举类型

```cpp
CursorDirection d = CursorDirection::kLeft;  // OK in C++11
int i = CursorDirection::kRight;             // OK: still converts to int
```

 但是重新定义枚举值仍然会出现重定义错误。


## Scoped Enumerations

据观察，隐式转换为整型是常见的bug来源，而由于枚举值和枚举类型是在统一scope内的，从而导致命名空间污染会给大型多库项目带来问题。为了解决这两个问题
C++11引入了一个新的概念: 作用域枚举。

作用域枚举，通过关键字`enum class`来定义，枚举值

* 只在对应枚举类型范围内生效
* 没办法隐式转换为整型

例如下面这个例子:

```cpp
enum class CursorDirection { kLeft, kRight, kUp, kDown };
CursorDirection d = kLeft;                    // error: kLeft not in this scope
CursorDirection d2 = CursorDirection::kLeft;  // OK
int i = CursorDirection::kRight;              // error: no conversion
```

还有这个例子:

```cpp
// OK: kLeft and kRight are local to each scoped enum
enum class PoliticalOrientation { kLeft, kCenter, kRight };
```

这些简单的更改消除了普通枚举的问题，因此在所有新代码中首选枚举类。

使用scoped enum意味着如果您仍然需要这样的转换，则必须要显示的将其转换为整型，（例如，在记录枚举值时，或对类似标志的枚举值使用按位运算时），
使用`std::hash`进行散列将继续工作（例如，`std::unordered_map<CursorDirection，int>`）。

## Underlying Enumeration Types

C++ 11还引入了为两种枚举类型指定基础类型的能力。以前，枚举的基本整数类型是通过检查枚举数的符号和大小来确定的，但现在我们可以显式了。例如，…

```cpp
// Use "int" as the underlying type for CursorDirection
enum class CursorDirection : int { kLeft, kRight, kUp, kDown };
```

因为这个枚举值范围很小，如果我们希望避免在存储`cursordirection`的值时浪费空间，我们可以指定其底层存储类型为`char`。

```cpp
// Use "char" as the underlying type for CursorDirection
enum class CursorDirection : char { kLeft, kRight, kUp, kDown };
```

如果枚举值超过了底层存储类型的大小范围，编译器会报告错误。

## Conclusion

在新的代码中，优先使用`enum class`，这将会减少名称污染，并且可以避免因为隐藏式转换带来的bug。

```cpp
enum class Parting { kSoLong, kFarewell, kAufWiedersehen, kAdieu };
```
