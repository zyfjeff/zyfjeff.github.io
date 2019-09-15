## Tip of the Week #99: Nonmember Interface Etiquette

> Originally posted as totw/99 on 2015-06-24
> Revised 2017-10-10

`C++`类的接口不限于其成员或其定义。在评估API时，我们必须考虑类主体之外的定义，这些定义可以作为其公共成员的接口的一部分。

这些外部接口点包括模板特化，如哈希或traits，非成员运算符重载（例如日志记录，关系），以及设计用于参数依赖查找（ADL）的其他规范非成员函数，最值得注意的是`swap()`。

下面举例说明了一些示例类`space::Key`:

```cpp
namespace space {
class Key { ... };

bool operator==(const Key& a, const Key& b);
bool operator<(const Key& a, const Key& b);
void swap(Key& a, Key& b);

// standard streaming
std::ostream& operator<<(std::ostream& os, const Key& x);

// gTest printing
void PrintTo(const Key& x, std::ostream* os);

// new-style flag extension:
bool ParseFlag(const string& text, Key* dst, string* err);
string UnparseFlag(const Key& v);

}  // namespace space

HASH_NAMESPACE_BEGIN
template <>
struct hash<space::Key> {
  size_t operator()(const space::Key& x) const;
};
HASH_NAMESPACE_END
```

错误地进行此类扩展会带来一些重要的风险，因此本文将尝试提供一些指导。

## The Proper Namespace

作为函数的接口点，通常设计为通过依赖与参数的查找(ADL, [TotW 49](https://abseil.io/tips/49))，来找到是由ADL设计的，运算符和一些类似运算符的函数(特别是swap())。只有在与要自定义的类型关联的命名空间中定义函数时，此协议才能可靠地工作或者是关联的命名空间包括其基类和类模版参数的命名空间。一个常见的错误是将这些函数放在全局命名空间中。为了说明问题，请考虑以下代码，其中
`good(x)`和`bad(x)`函数使用相同的语法调用：

```cpp
namespace library {
struct Letter {};

void good(Letter);
}  // namespace library

// bad is improperly placed in global namespace
void bad(library::Letter);

namespace client {
void good();
void bad();

void test(const library::Letter& x) {
  good(x);  // ok: 'library::good' is found by ADL.
  bad(x);  // oops: '::bad' is hidden by 'client::bad'.
}

}  // namespace client
```

注意：`library::good()`和`::bad()`的不同。`test()`函数依赖于在包含调用点所在的名称空间中没有任何名为`bad()`的函数。
`client::bad()`的存在将`::bad()`隐藏了起来，与此同时无论`test()`函数的在哪个作用域范围，都可以找到`good()`函数。C++的名称查找并不会逐级向更大的范围去查找名称
仅在调用处最近的范围内查找，如果查找不到就直接匹配全局的名称。

这一切都非常微妙，这点非常重要。如果我们默认将函数与它们运行的​​数据一起定义，那就简单多了。

## A Quick Note on In-Class Friend Definitions

有一种方法可以在类定义中向类中添加非成员函数。友元函数就可以直接在类中定义。

```cpp
namespace library {
class Key {
 public:
  explicit Key(string s) : s_(std::move(s)) {}
  friend bool operator<(const Key& a, const Key& b) { return a.s_ < b.s_; }
  friend bool operator==(const Key& a, const Key& b) { return a.s_ == b.s_; }
  friend void swap(Key& a, Key& b) {
    swap(a.s_, b.s_);
  }

 private:
  std::string s_;
};
}  // namespace library
```

这些友元函数具有仅通过ADL可见的特殊属性。它们有点奇怪，因为它们是在封闭的命名空间中定义的，因此不会出现在名称查找中。
这些类内友元定义必须具有内联主体才能启动此隐形属性。有关更多详细信息，请参阅“[友元定义](https://stackoverflow.com/questions/381164/friend-and-inline-method-whats-the-point)”。

这样的函数不会隐藏对于全局函数的调用，也不会出现在对同名函数的不相关调用的诊断信息中。从本质上说，它们是不会挡道的。它们的定义也非常方便，易于发现，并且
可以访问类内部。最大的缺点可能是当参数类型可以隐式转换的时候，无法通过ADL找到它们。

请注意，访问（即公共，私有和受保护）对友元功能没有影响，但无论如何将它们放在公共部分可能是礼貌的，只是因为它们在阅读公共API点时会更加明显。


## The Proper Source Location

为了避免违反一个定义规则（ODR），类型接口上的任何自定义函数 都应该出现在不能意外地多次定义它们的地方。这通常意味着它们应该与类型打包在同一个头中。
在*u test.cc文件或“utilities”头文件中添加此类非成员自定义项是不合适的，因为这样做可能会被忽略。强制编译器查看自定义项，则更有可能捕获冲突。