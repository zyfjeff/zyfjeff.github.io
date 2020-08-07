## Tip of the Week #126: `make_unique` is the new `new`

> Originally posted as totw/126 on 2016-12-12
> By James Dennett (jdennett@google.com) based on a mailing list post by Titus Winters (titus@google.com)

随着代码库的扩展，我们越来越难了解我们所依赖的所有细节。即使拥有深厚的知识也不能解决问题：无论是在编写代码还是在code review时，
我们都必须依靠接口和契约来知道代码正确。在许多情况下，类型系统可以以更通用方式提供这些契约。 一致的使用类型系统契约，可以通过识别分配
在堆上的对象和所有权转移的位置来简化代码的编写和检查。

尽管在`C++`中，我们可以通过使用plain value来减少对动态内存分配的需求，但有时我们需要动态对象来延长其作用域。
动态分配对象时，`C++`代码应首选智能指针（最常见的是`std::unique_ptr`），而不是原始指针。这提供了一个关于分配和所有权转移的一致的视角，
并在需要对所有权问题进行更仔细检查的代码处留下了更清晰的视觉信号。

两个关键工具，一个是`absl::make_unique`(在`C++11`时代实现了C++14中的`std::make_unique`，提供了一个无泄漏版本的内存分配)。
另外一个工具是`absl::WrapUnique`(用于将具有所有权的裸指针包装成`std::unique_ptr`类型)，他的实现可以在`absl/memory/memory.h`中发现。

### Why Avoid new?

为什么在代码中应该优先使用智能指针代替通过new来分配的裸指针。

1. 如果可能的化，在类型系统中所有权是最好的表达。他允许


### How Should We Choose Which to Use?

### Summary

`absl::make_unique`和`absl::WrapUnique`相比，优先使用`absl::make_unique`，而`absl::WrapUnique`和`new`相比，优先使用`absl::WrapUnique`

