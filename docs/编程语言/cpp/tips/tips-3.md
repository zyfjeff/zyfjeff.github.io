## Tip of the Week #3: String Concatenation and operator+ vs. StrCat()

> Originally published as totw/3 on 2012-05-11
> Updated 2017-09-18; revised 2018-01-22

当一个reviewer说"不要使用string的连接操作，这不高效"，通常会让提交代码的人很惊讶。`std::string::operator+`是如何低效的呢? 是不是很难弄错?

事实证明，这种低效并不明显，这两个片段在实际执行中有着接近相同的执行时间:

```c++
std::string foo = LongString1();
std::string bar = LongString2();
std::string foobar = foo + bar;

std::string foo = LongString1();
std::string bar = LongString2();
std::string foobar = absl::StrCat(foo, bar);
```

但是对于下面两个片段却不是如此:

```c++
std::string foo = LongString1();
std::string bar = LongString2();
std::string baz = LongString3();
string foobar = foo + bar + baz;

std::string foo = LongString1();
std::string bar = LongString2();
std::string baz = LongString3();
std::string foobar = absl::StrCat(foo, bar, baz);
```

当我们分解一下`foo + bar + baz`表达式中发生的情况，就可以理解上面两种情况有所不同的原因了。在C++中没有三个参数的操作符，所以必须要执行二次`string::operator+`操作才能完成三个字符串的相加。在两次调用之间会构造出一个临时的字符串因此`std::string foobar = foo + bar + baz`等同如下:

```c++
std::string temp = foo + bar;
std::string foobar = std::move(temp) + baz;
```

具体来说就是foo和bar的内容在放入foobar之前必须先复制到一个临时位置(有关`std::move`，看[Tip of the Week #77: Temporaries, moves, and copies](https://abseil.io/tips/77))。

`C++11`允许第二次连接操作的时候不需要创建一个新的`string`对象的：`std::move(temp) + baz` 等同于`std::move(temp.append(baz))`。然后有可能其内部buffer大小不够导致内存重新分配(会导致额外的拷贝)，因此在最坏的情况下，`n`字符串连接的时候需要`O(n)`次内存重分配。

一个好的替代方法就是使用`absl::StrCat()`，一个不错的帮助函数其实现在 [absl/strings/str_cat.h](https://github.com/abseil/abseil-cpp/blob/master/absl/strings/str_cat.h)文件中，通过计算必要的字符串长度，预先分配大小，并将所有输入数据进行连接，其复杂度优化到`O(n)`，同样对于以下情况：

```c++
foobar += foo + bar + baz;
```

使用`abs::StrAppend`可以带来同样的优化:

```c++
absl::StrAppend(&foobar, foo, bar, baz);
```

同样，`absl::StrCat()` and `absl::StrAppend()`对除了字符串类型意外的类型进行操作: 可以使用`absl::StrCat`/`absl::StrAppend`对`int32_t`, `uint32_t`, `int64_t`, `uint64_t`, `float`, `double`, `const char*`, and `string_view`等类型进行转换，像如下这样：

```c++
std::string foo = absl::StrCat("The year is ", year);
```