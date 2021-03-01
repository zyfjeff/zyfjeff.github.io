---
hide:
  - toc        # Hide table of contents
---
# Tip of the Week #94: Callsite Readability and bool Parameters

> Originally posted as totw/94 on 2015-04-27
> By Geoff Romer (gromer@google.com)
> Revised 2017-10-25

> “Of the many forms of false culture, a premature converse with abstractions is perhaps the most likely to prove fatal to the growth of …
> vigour of intellect.” — George Boole.

假设你遇到这样的代码:

```cpp
int main(int argc, char* argv[]) {
  ParseCommandLineFlags(&argc, &argv, false);
}
```

你可以告诉我这段代码是什么意思吗? 特别是最后一个参数的含义? 现在假设您之前遇到过这个函数，并且您知道最后一个参数与调用后是否在argv中保留命令行标志有关。你能说出什么情况下是true的，什么情况下是false呢？

当然你不知道，因为这是假设，但即使在真实的代码中，与记住每个函数参数的含义相比，和花时间去查找我们遇到的每个函数调用的文章相比，我们的大脑有更好的事情要做，我们必须能够通过查看调用点来对函数调用的含义做出相当好的猜测。

精心选择的函数名是使函数调用可读性的关键，但它们通常是不够的。我们通常还需要提供一些关于它们的含义的线索。例如，如果你在此之前从未看到过`string_view`，那么您可能不知道如何构造`absl::string_view s(x, y)`，但是`absl::string_view s(my_str.data(), my_str.size());` 和 `absl::string_view s("foo");` 更加清晰。bool参数的问题在于，调用点处的参数通常是字面值true或false，并且这使得读者没有关于参数含义的上下文提示，正如我们在`ParseCommandLineFlags（）`示例中看到的那样。如果存在多个bool参数，则此问题更加复杂，因为现在您还有另外一个问题，即确定哪个参数是哪个用途。

那么我们如何修复这个例子的代码呢？一个（不太好）可能性是做这样的事情：

```cpp
int main(int argc, char* argv[]) {
  ParseCommandLineFlags(&argc, &argv, false /* preserve flags */);
}
```

这种方法的缺点是显而易见的：不清楚该comment是否描述了参数的含义或参数的效果。换句话说，它是说我们保留了flag，还是说我们保留的flag是false？即使comment设法明确说明，comment仍然存在与代码不同步的风险。

更好的方法是在注释中指定参数的名称：

```cpp
int main(int argc, char* argv[]) {
  ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/false);
}
```

这更加清晰，并且不太可能与代码失去同步。 `Clang-tidy`甚至会检查注释是否具有正确的参数名称。较不模糊但较长的变体是使用变量来解释：

```cpp
int main(int argc, char* argv[]) {
  const bool remove_flags = false;
  ParseCommandLineFlags(&argc, &argv, remove_flags);
}
```

但是，编译器不会检查解释变量名称，因此它们可能是错误的。当您有多个bool参数时，这是一个特殊问题，可能会被调用者调换。

所有这些方法还依赖于程序员始终记住添加这些注释或变量，并正确地执行此操作（尽管clang-tidy将检查参数名称注释的正确性）。

在许多情况下，最好的解决方案是首先避免使用bool参数，而是使用枚举。例如，`ParseCommandLineFlags（）`可以像这样声明：

```cpp
enum ShouldRemoveFlags { kDontRemoveFlags, kRemoveFlags };
void ParseCommandLineFlags(int* argc, char*** argv, ShouldRemoveFlags remove_flags);
```

最终可能像下面这样调用:

```cpp
int main(int argc, char* argv[]) {
  ParseCommandLineFlags(&argc, &argv, kDontRemoveFlags);
}
```

你也可以使用枚举类，如[TotW 86](https://abseil.io/tips/86)中所述，但在这种情况下，您需要使用稍微不同的命名约定，例如

```cpp
enum class ShouldRemoveFlags { kNo, kYes };
…
int main(int argc, char* argv[]) {
  ParseCommandLineFlags(&argc, &argv, ShouldRemoveFlags::kNo);
}
```

显然，这个方法必须在定义函数时实现;你不能在调用时真正选择它（你可以伪造它，但收效甚微）。因此，当您定义一个函数时，特别是如果它将被广泛使用，那么您有责任仔细考虑如何调用，特别是对bool参数持怀疑态度。