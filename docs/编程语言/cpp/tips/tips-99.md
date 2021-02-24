## Tip of the Week #99: Nonmember Interface Etiquette

> Originally posted as totw/99 on 2015-06-24
> Revised 2017-10-10

`C++`类的接口不限于其成员或其定义。在评估API时，我们必须考虑除了类主体外的其他定义，这些定义与其公共成员一样，可能是其接口的一部分。

这些外部接口包括hasher和traits的模版特化、非成员操作符(例如输出和关系运算符等)，还有一些其他一些设计用于借助ADL(argument-dependent lookup)的规范成员函数。其中最值得关注的是`swap()`

下面例举了一些示例类: `space::Key`

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

错误地进行这类扩展会带来一些重要的风险，因此本文将尝试提供一些指导。

## The Proper Namespace

通常，设计为非成员函数的接口可以通过ADL(依赖于参数所在namespace来查找)来找到（ADL，请参见[TotW 49](https://abseil.io/tips/49)）其定义。运算操作符函数和类似操作符的一些函数(最值得注意的是swap)
这些都是设计成通过ADL来查找的。而ADL工作的前提是这些函数的定义要和其要操作的自定义类型在一个namespace中。相关联的namespace包括其基类和类模板的参数。一个常见的错误是把这些函数放在全局命名空间。为了说明这个问题，考虑下面的代码中的`good(x)`和`bad(x)`函数的调用是正确的。

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

注意`library::good()`和`::bad()`之间的区别。`test()`函数依赖于包含调用点所在的namespace中缺少任何名为`bad()`的函数，`client::bad`将`::bad`隐藏了。
与此同时，无论`test()`函数及其范围中存在什么其他元素，都可以找到正确`good()`函数。