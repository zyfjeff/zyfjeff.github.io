## Tip of the Week #10: Splitting Strings, not Hairs

> Originally published as totw/10 on 2012-08-16
> *By Greg Miller (jgm@google.com)*
> Updated 2018-01-24
> *I tend to have an odd split in my mind. –John Cleese*

在任何通用目的的编程语言中将一个`string`切割成一个子串是一个很常见的任务，`C++`也不例外。**当谷歌出现这种需求时，许多工程师发现自己需要通过扩展头文件的方式来扩展分割字符串的功能。你需要寻找满足您需求的输入参数、输出参数和语义的神奇组合。在研究了600多行标题中的50多个函数以后，你可能错误的将新的分割函数命名为`SplitStringViewToDequeOfStringAllowEmpty()`**。

为了处理这个问题，`C++`库的团队实现了一个新的`API`用于实现字符串分割的功能，其头文件在[absl/strings/str_split.h](https://github.com/abseil/abseil-cpp/blob/master/absl/strings/str_split.h)。

这个新的`API`将许多功能类似的分割函数替换成了一个单个的 `absl::StrSplit()`函数。这个函数接收一个将被分割的`string`和一个分隔符作为输入，`absl::StrSplit()`返回结果将会适配调用者指定的类型。`absl::StrSplit`的实现是高效的，因为其内部广泛使用了`absl::string_view`。除非调用者明确将结果存储在字符串对象的集合中(会复制其数据)，否则不会复制数据的。

说够了，让我们看一些例子：

```c++
// Splits on commas. Stores in vector of string_view (no copies).
std::vector<absl::string_view> v = absl::StrSplit("a,b,c", ',');

// Splits on commas. Stores in vector of string (data copied once).
std::vector<std::string> v = absl::StrSplit("a,b,c", ',');

// Splits on literal string "=>" (not either of "=" or ">")
std::vector<absl::string_view> v = absl::StrSplit("a=>b=>c", "=>");

// Splits on any of the given characters (',' or ';')
using absl::ByAnyChar;
std::vector<std::string> v = absl::StrSplit("a,b;c", ByAnyChar(",;"));

// Stores in various containers (also works w/ absl::string_view)
std::set<std::string> s = absl::StrSplit("a,b,c", ',');
std::multiset<std::string> s = absl::StrSplit("a,b,c", ',');
std::list<std::string> li = absl::StrSplit("a,b,c", ',');

// Equiv. to the mythical SplitStringViewToDequeOfStringAllowEmpty()
std::deque<std::string> d = absl::StrSplit("a,b,c", ',');

// Yields "a"->"1", "b"->"2", "c"->"3"
std::map<std::string, std::string> m = absl::StrSplit("a,1,b,2,c,3", ',');
```

更多的细节相关的信息可以去看[absl/strings/str_split.h](https://github.com/abseil/abseil-cpp/blob/master/absl/strings/str_split.h)，对于如何使用`Split`的`API`可以看[absl/strings/str_split_test.cc](https://github.com/abseil/abseil-cpp/blob/master/absl/strings/str_split_test.cc)里面有很多`examples`。