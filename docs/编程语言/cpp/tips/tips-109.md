---
hide:
  - toc        # Hide table of contents
---
# Tip of the Week #109: Meaningful `const` in Function Declarations

> Originally posted as totw/109 on 2016-01-14
> By Greg Miller (jgm@google.com)

本文档将说明const在函数声明中何时有意义，当它毫无意义，最好省略。但是首先，让我们简要解释一下声明和定义这两个术语的含义。

考虑下面这段代码:

```cpp
void F(int);                     // 1: declaration of F(int)
void F(const int);               // 2: re-declaration of F(int)
void F(int) { /* ... */ }        // 3: definition of F(int)
void F(const int) { /* ... */ }  // 4: error: re-definition of F(int)
```

前两行是函数的声明，一个函数的声明就是告诉编译器这个函数的签名和返回类型。在上面的例子中，这个函数的签名是`F(int)`。
函数参数类型的常量性将被忽略，因此前两个声明都是等效的。(具体细节见 ["Overloadable declarations”](http://eel.is/c++draft/over.load))

上面例子中第三和第四行是函数的定义。一个函数的定义也是一个声明。但是定义包含了函数的实现。因此第三行是具有签名为`F(int)`的函数定义。第四行也是
对相同函数`F(int)`的定义。这将会导致链接时错误，因为多个函数声明是允许的，但是只能有一个定义。

尽管第三行和第四行都是相同函数的定义，但是他们的实现确实不同的，这取决于他们的声明方式。第三行的函数参数是`int`(非`const`)，第四行的函数参数是`const int`。

## Meaningful const in Function Declarations

函数声明中并不是所有的const限制符都可以忽略，引用自`C++`标准中的 "Overloadable声明"（[over.load])。

在参数类型的const类型说明符很重要，可用于区分重载的函数声明



## Rules of Thumb