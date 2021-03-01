---
hide:
  - toc        # Hide table of contents
---
# Tip of the Week #119: Using-declarations and namespace aliases

> Originally posted as totw/119 on 2016-07-14
> By Thomas Köppe (tkoeppe@google.com)

本Tips为在`.cc`文件中编写using声明和名称空间别名提供了一个简单而有效的方法，该方法避免了一些细微的陷阱

深入研究细节之前，先看一下，下面这个示例：

```cpp
namespace example {
namespace makers {
namespace {

using ::otherlib::BazBuilder;
using ::mylib::BarFactory;
namespace abc = ::applied::bitfiddling::concepts;

// Private helper code here.

}  // namespace

// Interface implementation code here.

}  // namespace makers
}  // namespace example
```

记住，这个Tips所讨论的范围仅仅在是`.cc`文件中，因为您绝不应该在头文件中放置别名，这样的别名为实现者（和实现的读者）提供了便利，
而不是为了暴露给使用者的便利。(当然，可以在头文件中声明属于导出的API的名称别名。)

## Summary

* 不要在头文件中声明namespace别名或者是方便使用的using声明，仅在`.cc`中声明
* 在内部的命名空间（无论是命名的还是匿名的）中声明命名空间别名和using声明。 （请勿仅为此目的添加匿名空间。）
* 声明namespace空间别名和using声明时，除非您在当前名称空间中引用名称，否则请使用完全限定名称（带有前导`::`）。
* 有关名称的其他用途，请在合理的情况下避免完全限定，请参阅[TotW 130](https://abseil.io/tips/130)

> 请记住，您始终可以在块作用域中拥有本地namespace别名或using声明，这在仅有头文件的库中非常方便

## Background

`C++`将名称组织到名称空间中。这种至关重要的功能允许通过将名称所有权保持在本地来扩展代码库，从而避免在其他范围内发生名称冲突。但是，由于限定名称(`foo::bar`)通常很长且很快变得混乱，
因此命名空间会带来一定的外观负担。我们经常发现使用非限定名称(`Bar`)很方便，另外，我们可能希望为一个长期但经常使用的命名空间引入一个命名空间别名：`namespace eu = example::v1::util`。
在本文中，我们将统称为using声明和namespace别名。

## The Problem

命名空间的目的是帮助代码作者在名称查找和链接时避免名称冲突。别名可能会破坏名称空间提供的保护。这个问题有两个不同的方面：别名的范围和相对限定符的使用。

### Scope of the Alias

别名放置的范围可能会对代码的可维护性产生微妙的影响。考虑以下两个变体：

```cpp
using ::foo::Quz;

namespace example {
namespace util {

using ::foo::Bar;
```

使用using声明使得`Quz`和`Bar`可以很好的在我们的`::example::util`命名空间中工作，避免使用完全限定，通过非限定名称查询`Bar`和`Quz`。对于`Bar`，
只要命名空间`::example::util`中没有其他`Bar`声明，一切都会按预期进行。因为这是您的namespace空间，因此你可以控制它，不会出现名称冲突。
另一方面，如果以后包含了一个头文件，并且这个头文件也声明了一个全局的名称`Quz`，那么这个`Quz`的using声明将会变得不正确了。因为它试图重新声明名称`Quz`。
如果另一个头文件中声明了`::example::Quz`或`::example::util::Quz`，则未限定查找将找到这个名称而不是您的别名

如果您不将名称添加到您不拥有的namespace空间（包括全局名称空间）中，则可以避免这种脆弱性。通过将别名放置在您自己的namespace空间中，未限定的查找将首先查找您的别名，
并且永远不会继续搜索包含namespace空间的内容。

通常，声明越接近使用点，则可能破坏代码的范围集越小，在我们例子中最糟糕的一个是`Quz`，任何人都可以打破它。 `Bar`只能被`::example::util`中的其他代码破坏，
并且在未命名空间中声明和使用的名称不能被任何其他范围破坏。有关示例，请参[见Unnamed Namespaces](https://abseil.io/tips/119#unnamed-namespaces)。


### Relative Qualifiers

使用`foo::`Bar的形式的using声明似乎是无害的，但实际上是模棱两可的。问题在于，依靠namespace中已经存在的名称是安全的，但是依靠不存在的名称是不安全的。考虑以下代码:

```cpp
namespace example {
namespace util {

using foo::Bar;
```

上面的代码依赖于`::foo::Bar`的存在或者是不存在名称空间`::example::foo`和`::example::util::foo`，可以通过完全限定所使用的名称来避免这种脆弱性：使用`::foo::Bar`。
相对名称唯一且不能被外部声明破坏的唯一条件是它是否引用了当前namespace中已经存在的名称：

```cpp
namespace example {
namespace util {
namespace internal {

struct Params { /* ... */ };

}  // namespace internal

using internal::Params;  // OK, same as ::example::util::internal::Params
```

下面遵循了上一节中讨论的相同逻辑

如果名称位于同级命名空间中，例如`::example::tools::Thing`，该怎么办？您可以说`tools::Thing`或`::example::tools::Thing`。完全限定的名称始终是正确的，但使用相对名称也可能是适当的。自己来判断。
避免许多此类问题的廉价方法是不在项目中使用与流行的顶级namespace空间（例如util）相同的namespace空间；[样式指南](https://google.github.io/styleguide/cppguide.html#Namespace_Names)明确建议这种做法。

### Demo

以下代码显示了两种错误模式的示例。

**helper.h:**

```cpp
namespace bar {
namespace foo {

// ...

}  // namespace foo
}  // namespace bar
```


**some_feature.h:**

```cpp
extern int f;
```

**Your code:**

```cpp
#include "helper.h"
#include "some_feature.h"

namespace foo {
void f();
}  // namespace foo

// Failure mode #1: Alias at a bad scope.
using foo::f;  // Error: redeclaration (because of "f" declared in some_feature.h)

namespace bar {

// Failure mode #2: Alias badly qualified.
using foo::f;  // Error: No "f" in namespace ::bar::foo (because that namespace was declared in helper.h)

// The recommended way, robust in the face of unrelated declarations:
using ::foo::f;  // OK

void UseCase() { f(); }

}  // namespace bar
```

## Unnamed Namespaces

可以从封闭的namespace中访问放置在unamed namespace中的using声明，反之亦然。如果文件顶部已经有一个unamed namespace，
则最好在其中放置所有别名。从该unamed namespace中，您可以获得额外的健壮性，可以避免与封闭的namespace中声明的内容发生冲突。

```cpp
namespace example {
namespace util {

namespace {

// Put all using-declarations in here. Don't spread them over the file.
using ::foo::Bar;
using ::foo::Quz;

// In here, Bar and Quz refer inalienably to your aliases.

}  // namespace

// Can use both Bar and Quz here too. (But don't declare any entities called Bar or Quz yourself now.)
```

## Non-aliased names

到目前为止，我们一直在谈论对非当前文件的namespace进行别名。但是，如果我们想直接使用名称而不是创建别名怎么办？我们应该说`util::Status`还是`::util::Status`？
没有明显的答案。不同于我们到目前为止讨论过的别名声明，它们出现在文件的顶部，与实际代码相距甚远，而名称的直接使用会影响代码的本地可读性。确实，相对名称将来可能会取消，
但是使用完全限定的名称代价很高。前导`::`符号引起的视觉混乱可能会分散注意力，不值得增加鲁棒性。在这种情况下，请根据自己的判断来决定您喜欢哪种样式。参见[TotW 130](https://abseil.io/tips/130)

## Acknowledgments

一切归功于Roman Perepelitsa（romanp@google.com），他最初在邮件列表讨论中提出了这种风格，并做出了许多更正和强调。但是，所有错误都是我的。