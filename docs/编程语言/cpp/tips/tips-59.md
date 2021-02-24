## Tip of the Week #59: Joining Tuples
> Originally published as totw/59 on 2013-10-21
> By Greg Miller (jgm@google.com)
> Updated 2018-01-24
> “Now join your hands, and with your hands your hearts.” –Henry VI, William Shakespeare
> 

2013年的三月我们在[Tips #36](https://abseil.io/tips/36)中宣布了一个新的[string joining](https://github.com/abseil/abseil-cpp/blob/master/absl/strings/str_join.h) API，大家对于新的API的反馈是非常积极的，我们努力使这个API更好。在反馈的意见中最重要的是提供连接不同类型数据的功能。我们没有使用可变参数或者是可变模版的方式来实现，而是选择支持`std::tuple`，通过`std::tuple`间接的来支持。这很好的满足了需求。只需要创建一个包含不同类型数据的`std::tuple`即可，`absl::StrJoin`会像其它任何容器一样接收它。这里有一些例子，如下:

```cpp
auto tup = std::make_tuple(123, "abc", 0.456);
std::string s = absl::StrJoin(tup, "-");
s = absl::StrJoin(std::make_tuple(123, "abc", 0.456), "-");

int a = 123;
std::string b = "abc";
double c = 0.456;

// Works, but copies all arguments.
s = absl::StrJoin(std::make_tuple(a, b, c), "-");
// No copies, but only works with lvalues.
s = absl::StrJoin(std::tie(a, b, c), "-");
// No copies, and works with lvalues and rvalues.
s = absl::StrJoin(std::forward_as_tuple(123, MakeFoo(), c), "-");
```
和其他任何容器一样，默认`tuple`中的元素是通过`absl::AlphaNumFormatter`这个formatted来进行格式化的，如果你的`tuple`中包含了默认`Formatter`无法格式化的类型，你可以自定义[Formatter](https://github.com/abseil/abseil-cpp/blob/master/absl/strings/str_join.h#L64)。你自定义的`Formatter`对象可能需要包含多个`operator`重载。例如:

```cpp

struct Foo {};
struct Bar {};

struct MyFormatter {
  void operator()(string* out, const Foo& f) const {
    out->append("Foo");
  }
  void operator()(string* out, const Bar& b) const {
    out->append("Bar");
  }
};

std::string s = absl::StrJoin(std::forward_as_tuple(Foo(), Bar()), "-",
                         MyFormatter());
EXPECT_EQ(s, "Foo-Bar");
```

`absl::StrJoin`API的目标是通过一致的语法来join任何集合、range、list或一组数据。我们认为加入`std::tuple`对象非常适合这个目标，并未API增加了更多的灵活性。