---
hide:
  - toc        # Hide table of contents
---
# Tip of the Week #24: Copies, Abbrv
> Originally posted as TotW #24 on Nov 26, 2012
> by Titus Winters, (titus@google.com) and Chandler Carruth (chandlerc@google.com)
> “To copy others is necessary, but to copy oneself is pathetic.” - Pablo Picasso

**注意**: 有关名称计数和副本与移动的指导，请参见[TotW 77](https://abseil.io/tips/77)和[TotW 55](https://abseil.io/tips/55)

## One Name, No Copy; Two Names, Two Copies
当评估是否在任何给定范围内是否有副本(包括触发`RVO`的情况)时，请检查你的数据被多少个变量名引用。

在任何时候如果一个数据有两份副本的话，那么你一定拥有两个有效的名字来引用这两个副本。其他情况下编译器会负责删除副本。


## Examples
让我们来看下面这个例子，看看实际情况下是如何运作的:

```cpp
std::string build();

std::string foo(std::string arg) {
  return arg;  // no copying here, only one name for the data “arg”.
}

void bar() {
  std::string local = build();  // only 1 instance -- only 1 name

  // no copying, a reference won’t incur a copy
  std::string& local_ref = local;

  // one copy operation, there are now two named collections of data.
  std::string second = foo(local);
}
```
大多数情况下这都不重要，确保您的代码可读并且一致是非常重要的。而不是担心副本拷贝和性能。一如既往，不要过早优化。但是如果你发现从头开始编写代码的话你可以提供一个更加干净和一致的API返回值，不要因为它可能会带来拷贝而妥协，你在十年前用`C++`学习过的所有内容都是错误的。

> As always: profile before you optimize. But, if you find yourself writing code from scratch – and can provide a clean and consistent API that returns its values – don’t discount code that seems like it would make copies: everything you learned about copies in C++ a decade ago is wrong.
> 最后一段翻译的不太好，如果有更好的翻译，请告知我。