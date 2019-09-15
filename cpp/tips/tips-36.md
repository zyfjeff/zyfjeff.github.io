## Tip of the Week #36: New Join API

> Originally published as totw/36 on 2013-03-21
> *By Greg Miller (jgm@google.com)*
> Updated 2018-01-24
> *“I got a good mind to join a club and beat you over the head with it.” – Groucho Marx*

你们许多人要求提供新的`Join API`，并且我们也听到你的声音。我们现在有一个`Join`的函数来替换他们所有。它拼写为`absl::StrJoin`，你只需要给它一个要加入的对象和一个分隔符串的集合，剩下的就完成了。它将与`std::string`、`absl::string_view`、`int`、`double`的集合一起工作。任何`absl::StrCat`所支持的类型`absl::StrJoin`都支持，**对于不支持的类型可以通过提供一个定制的`Formatter`来支持;我们将在下面看到如何使用`Formatter`来让我们很好地加入对map类型的支持**。

现在来举一些简单的例子：

```cpp
std::vector<std::string> v = {"a", "b", "c"};
std::string s = absl::StrJoin(v, "-");
// s == "a-b-c"

std::vector<absl::string_view> v = {"a", "b", "c"};
std::string s = absl::StrJoin(v.begin(), v.end(), "-");
// s == "a-b-c"

std::vector<int> v = {1, 2, 3};
std::string s = absl::StrJoin(v, "-");
// s == "1-2-3"

const int a[] = {1, 2, 3};
std::string s = absl::StrJoin(a, "-");
// s == "1-2-3"
```

下面的例子通过添加一个`Formatter`函数并使用了一个不通的分隔符来格式化map类型，这使得输出结果更好并且可读。

```c++
std::map<std::string, int> m = {{"a", 1}, {"b", 2}, {"c", 3}};
std::string s = absl::StrJoin(m, ";", absl::PairFormatter("="));
// s == "a=1;b=2;c=3"
```

你也可以传递一个C++的lambda表达式作为`Formatter`。

```c++
std::vector<Foo> foos = GetFoos();

std::string s = absl::StrJoin(foos, ", ", [](std::string* out, const Foo& foo) {
  absl::StrAppend(out, foo.ToString());
});
```

请参考 [absl/strings/str_join.h](https://github.com/abseil/abseil-cpp/blob/master/absl/strings/str_join.h)来获取更多细节。