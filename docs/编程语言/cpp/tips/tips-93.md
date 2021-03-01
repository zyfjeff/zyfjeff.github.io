---
hide:
  - toc        # Hide table of contents
---
# Tip of the Week #93: using absl::Span

> Originally posted as TotW #93 on April 23, 2015
> by Samuel Benzaquen (sbenza@google.com)

在Google的时候，当我们要处理`unowned strings`(不拥有字符串的生命周期)的时候，我们惯例会去使用`absl::string_view`作为函数参数和返回类型。通过它
可以使得API更灵活，而且还可以通过避免不必要的`std::string`转换来提高性能。(See [TotW #1](https://abseil.io/tips/1))

`absl::string_view`有一个更通用的兄弟叫做`absl::Span`，`absl::Span<const T>`就是对`std::vector<T>`的引用，就好比`absl::string_view`和`std::string`的关系。
它提供了只读接口来访问`vector`中的元素。并且还可以从非vector类型进行构造(比如数组、初始化列表等)，而且不会带来元素拷贝的开销。

`const`可以去掉，`absl::Span<const T>`是对数组的一个只读的视图，其中的元素是不可变的，而`absl::Span<T>`则允许非const来访问元素，但这需要显示的构造`absl::Span<T>`。

## Note on std::span / gsl::span

值得注意的是，虽然`absl::Span`在设计和目的上与`std::span`提案（以及现有的`gsl::span`参考实现）类似，但是`absl::Span`目前不保证是任何最终标准的替代品，
因为`std::span`提案仍在开发中并且正在进行更改。

相反，`absl::Span`旨在使一个接口尽可能与`absl::string_view`相似，而不是特定于字符串的功能。

## As Function Parameters

使用`absl::Span`作为函数参数其优点类似于使用`absl::string_view`。

调用着可以传递vector的一部分、或者传递数组，只要是兼容类数组的容器就都可以。比如: `absl::InlinedVector`、`absl::FixedArray`、`google::protobuf::RepeatedField`等

和`absl::string_view`一样，通常使用`absl::Span`的值类型作为函数参数，这种从事要比通过传递const引用要更快(在大多数平台下)，可以产生更小的代码。

`absl::Span`的基本例子:

```cpp
void TakesVector(const std::vector<int>& ints);
void TakesSpan(absl::Span<const int> ints);

void PassOnlyFirst3Elements() {
  std::vector<int> ints = MakeInts();
  // We need to create a temporary vector, and incur an allocation and a copy.
  TakesVector(std::vector<int>(ints.begin(), ints.begin() + 3));
  // No copy or allocations are made when using Span.
  TakesSpan(absl::Span<const int>(ints.data(), 3));
}

void PassALiteral() {
  // This creates a temporary std::vector<int>.
  TakesVector({1, 2, 3});
  // Span does not need a temporary allocation and copy, so it is faster.
  TakesSpan({1, 2, 3});
}
void IHaveAnArray() {
  int values[10] = ...;
  // Once more, a temporary std::vector<int> is created.
  TakesVector(std::vector<int>(std::begin(values), std::end(values)));
  // Just pass the array. Span detects the size automatically.
  // No copy was made.
  TakesSpan(values);
}
```

## Buffer Overflow Prevention

因为`absl::Span`知道自己存储的元素的长度，因此它的API可以获得超过C风格的指针/长度组合所带来的安全性。

安全的`memcpy()`的例子

```cpp
// Bad code
void BadUser() {
  int src[] = {1, 2, 3};
  int dest[2];
  memcpy(dest, src, ABSL_ARRAYSIZE(src) * sizeof(int)); // oops! Dest overflowed.
}
```

```cpp
// A simple example, but takes advantage that the sizes of the Spans are known
// and prevents the above mistake.
template <typename T>
bool SaferMemCpy(absl::Span<T> dest, absl::Span<const T> src) {
  if (src.size() > dest.size()) {
    return false;
  }
  memcpy(dest.data(), src.data(), src.size());
  return true;
}

void GoodUser() {
  int src[] = {1, 2, 3}, dest[2];
  // No overflow!
  SaferMemCpy(absl::MakeSpan(dest), absl::Span<const int>(src));
}
```

## Const Correctness for Vector of Pointers

传递`std::vector<T*>`的最大问题在于你没办法在不改变容器类型的情况下让指针变成const。

任何函数使用`const std::vector<T*>&`作为参数虽然不能修改vector本身，但是却可以修改vector中的元素。这对于返回`const std::vector<T*>&`的场景也是相同的效果。
你无法阻止调用者来修改`T`的值。

通用的解决方案包括将vector拷贝、或者cast成正确的类型。这个解决方案很慢，并且会产生未定义行为，应该要避免。使用`absl::Span`来替代。


作为函数参数的例子:

考虑如下`Frob`的变种:

```cpp
void FrobFastWeak(const std::vector<Foo*>& v);
void FrobSlowStrong(const std::vector<const Foo*>& v);
void FrobFastStrong(absl::Span<const Foo* const> v);
```

上述变种前两个是不完美的，最后一个是好的。


```cpp
// fast and easy to type but not const-safe
FrobFastWeak(v);
// slow and noisy, but safe.
FrobSlowStrong(std::vector<const Foo*>(v.begin(), v.end()));
// fast, safe, and clear!
FrobFastStrong(v);
```

Accessor的例子:

```cpp
class DontDoThis {
 public:
  // Don’t modify my Foos, pretty please.
  const std::vector<Foo*>& shallow_foos() const { return foos_; }

 private:
  std::vector<Foo*> foos_;
};

void Caller(const DontDoThis& my_class) {
  // Modifies a foo even though my_class is a reference-to-const
  my_class->foos()[0]->SomeNonConstOp();
}
```

```cpp
// Good code
class DoThisInstead {
 public:
  absl::Span<const Foo* const> foos() const { return foos_; }

 private:
  std::vector<Foo*> foos_;
};

void Caller(const DoThisInstead& my_class) {
  // This one doesn't compile.
  // my_class.foos()[0]->SomeNonConstOp();
}
```


## Conclusion

适当的使用`absl::Span`，可以提供解耦，以及带来`const`正确性和性能的优点。

最重要的是注意到absl::Span的行为和absl::string_view很像，都是对数据的引用。同样，`absl::string_view`的使用上的一些警告，也适用于`absl::Span`
特别是一个`absl::Span`的生命周期不能超过它所引用的数据。