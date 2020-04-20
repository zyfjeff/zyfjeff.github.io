## Tip of the Week #107: Reference Lifetime Extension

> Originally posted as totw/107 on 2015-12-10
> By Titus Winters (titus@google.com)

关于[TotW 101](https://abseil.io/tips/107)之后，有一些关于引用和生命周期的不太清楚的研究，因此在本Tips中，我们将深入探讨“何时延长参考生命周期？”这一问题。

```cpp
string Foo::GetName();
const string& name = obj.GetName();  // Is this safe/legal?
```

简而言之，当且仅当

* 一个`const T&`(或者是`T&&`，尽管Google code style通常不建议使用)被一个返回临时对象`T`的表达式(通常是一个函数调用)初始化

标准术语的解释可能有些棘手，因此让我们讨论一些边缘情况来阐明：

* 当赋值给`T&`的时候是没办法工作的，必须是赋值给`const T&` (它会发生编译错误)

* 如果它是一个类型(非多态)的转换，例如将`string`赋值给`const absl::string_view&`，这并没有拓宽`string`的生命周期。

* 直接获取子对象也是没办法正常工作。

* 允许类型转换的情况是，当`T`是`U`的父类时，从`U`的临时对象赋值给`T＆`。请不要这样做：与其他情况相比，它甚至会使读者感到困惑。

如果临时对象的生命周期被拓展了，那么它将会持续到引用它的变量离开作用域。如果临时对象的生命周期没有被拓展，那么`T`将会在语句末尾的时候被销毁

通过 [TotW 101](https://abseil.io/tips/101) 我们可以知道，不应该依赖显示的引用初始化来进行生命周期的拓展，这并未得到多少性能的提升。而且这为代码可读性以及未来的可维护性带来不少困难。

一些细微的情况下，生命周期延长正在发生，而且是必要的，并且是有益的(例如在临时容器上进行for range)，同样，生命周期的扩展仅用于临时表达式的结果，而不用于任何子表达式。例如，这些工作：


```cpp
std::vector<int> GetInts();
for (int i : GetInts()) { }  // lifetime extension on the vector is important

// Return string_views of size 1 for each char in this string.
std::vector<absl::string_view> Explode(const string& s);

// vector中的string_view的生命周期并没有被延长
// Lifetime extension kicks in on the vector, but *not* on the temporary string!
for (absl::string_view s : Explode(StrCat("oo", "ps"))) { }  // WRONG
```

下面则无法正常工作:

```cpp
MyProto GetProto();

// sub_protos获取到的子对象没办法进行生命周期的延长，MyProto对象在语句结束时就已经被销毁了。
// Lifetime extension *doesn't work* here: sub_protos (a repeated field)
// is destroyed by MyProto going out of scope, and the lifetime extension rules
// don't kick in here to magically lifetime extend the MyProto returned by
// GetProto().  The sub-object lifetime extension only works for simple
// is-a-member-of relationships: the compiler doesn't see that sub_protos()
// itself returning a reference to an sub-object of the outer temporary.
for (const SubProto& p : GetProto().sub_protos()) { }  // WRONG
```