---
hide:
  - toc        # Hide table of contents
---
# Tip of the Week #117: Copy Elision and Pass-by-value

> Originally posted as TotW #117 on June 8, 2016
> by Geoff Romer, (gromer@google.com)
> “Everything is so far away, a copy of a copy of a copy. The insomnia distance of everything, you can’t touch anything and nothing can touch you.” — Chuck Palahniuk

假设你有一个像下面例子中的类:

```cpp
class Widget {
 public:
  …

 private:
  string name_;
};
```

你要如何给它编写构造函数? 多年来，答案一直都是像下面的代码一样:

```cpp
// First constructor version
explicit Widget(const std::string& name) : name_(name) {}
```

但是，有一种替代方法正变得越来越普遍:

```cpp
// Second constructor version
explicit Widget(std::string name) : name_(std::move(name)) {}
```

(如果你不熟悉`std::move`可以去看 [TotW #77](https://abseil.io/tips/77)，或者假装我用`std::swap`代替；相同的原则适用)
上面的代码到底发生了什么? 通过拷贝传递`std::string`难道不是很昂贵吗？事实证明，并不是这样的，有的时候进行值传递(如我们所见，并不是真正的拷贝)会比通过应用传递更高效。

要了解原因，可以考虑下面这段代码发生了什么:

```cpp
Widget widget(absl::StrCat(bar, baz));
```

我们第一个版本的构造函数中，`absl::StrCat`会产生一个临时的字符串，然后通过引用传递给`Widget()`，最后通过拷贝到`name_`。在第二个版本的`Widget`构造函数中，临时字符串
通过值传递的方式传递给构造函数，你可能认为这里会导致字符串拷贝，其实不然。当编译器看到一个临时字符串被拷贝构造成一个对象的时候，编译器将简单地对临时对象和新对象使用相同的存储
因此从一个到另一个的复制实际上是免费的；这称为`Copy elision`

能够使用`Copy elision`技术的前提是`name`参数的类型必须是能够被拷贝的。确实，此技术的本质是尝试使复制操作发生在可以消除复制函数的函数调用边界上，而不是在函数内部进行。
这不必涉及`std::move()`;。

## When to Use Copy Elision

通过值传递参数有几个缺点，应该牢记。第一它使函数主体更加复杂，从而造成维护和可读性负担，例如在上面的代码中，我们添加了一个`std::move`调用，这可能会导致意外访问已经被移动的值的风险，
在上面的函数中，风险很小，但如果函数更复杂，则风险会更高。第二它有时会以令人惊讶的方式降低性能。如果不分析特定的工作负载，有时可能很难说出为什么。

* 如上所述，该技术仅适用于需要复制的参数。如果将其应用于不需要复制或仅需要有条件复制的参数，则充其量是无用的，最坏的是有害的。

* 该技术通常在函数主体中涉及一些额外的工作，例如上面示例中的移动赋值。如果这些额外的工作增加了过多的开销，那么在无法省略拷贝的情况降低速度可能不值得在可以省略拷贝的地方加快速度。
  请注意，此确定可能取决于您的用例的细节，如果`Widget()`的参数几乎总是很短，或者几乎从来都不是临时的，那么这种技术总的来说可能是有害的。与以往一样，在考虑优化权衡时，如有疑问，请进行衡量。

* 当拷贝是通过赋值操作进行的时候(例如，如果我们想向`Widget`添加`set_name()`方法)，那么传递引用的版本有的时候可以避免内存分配，重用`name_`现有的buffer，而通过值传递的版本
  会导致分配新的内存，此外，值传递始终会替换`name_`的分配，这一事实可能会导致更糟糕的分配行为。如果`name_`字段在设置后趋于随时间增长，则在通过值传递的情况下这种增长将需要进一步的新分配，而在通过引用的情况下，我们仅在name_字段超过其历史最大值时才重新分配内存。

一般来说，您应该更喜欢更简单，更安全，更易读的代码，并且只有在有具体证据表明复杂版本的性能更好并且差异很重要的情况下，才选择更复杂的代码。该原理当然适用于该技术：通过const引用传递更简单，更安全，因此它仍然是一个不错的默认选择。但是，如果您在一个对性能敏感的区域工作，或者您的基准测试表明您花了太多时间来复制函数参数，那么按值传递可能是一个非常有用的工具。

> 严格来说，编译器不需要执行Copy elision，但是它是一项功能强大且至关重要的优化，因此您极有可能会遇到不执行该操作的编译器。