---
hide:
  - toc        # Hide table of contents
---
# Tip of the Week #64: Raw String Literals
> Originally published as totw/64 on 2013-12-09
> By Titus Winters (titus@google.com)
> Updated 2017-10-23
> Quicklink: [abseil.io/tips/64](https://abseil.io/tips/64)
> "(?:\"(?:\\\\\"|[^\"])*\"|'(?:\\\\'|[^'])*')"; — A cat walking over the keyboard . . . or maybe what the fox says . . . no, actually just a highly- escaped regexp found in real C++ code.
>

奇怪的是在C++中你可能会因为转义的问题导致你无法理解正常的正则表达式。同样，你在将`Protobuf`或者是`JSON`数据嵌入到单元测试中的文本时，您可能会因为要保留引号和换行符而感到恼火。当你必须使用转义(或者更为糟糕的多重转义的时)时，代码的清晰度会急剧下降。

幸运的是，有一个新的C++11的特性`raw string literals`，它消除了转义的需要。

## The Raw String Literal Format
一个`raw string literals`具有如下的语法格式:

```cpp
R"tag(whatever you want to say)tag"
```
`tag`是一个最多16个字符的序列(空的`tag`也是可以的)。`"tag`和`tag"`之间的字符串就是整个字符串的内容。`tag`可以包含除了`{}`、`\`和空白外的任何字符。可以看下，下面两端代码的差异：

```cpp
const char concert_17_raw[] =
    "id: 17\n"
    "artist: \"Beyonce\"\n"
    "date: \"Wed Oct 10 12:39:54 EDT 2012\"\n"
    "price_usd: 200\n";
```

```cpp
const char concert_17_raw[] = R"(
    id: 17
    artist: "Beyonce"
    date: "Wed Oct 10 12:39:54 EDT 2012"
    price_usd: 200)";
```

## Special Cases
注意锁进规则，当遇到`raw string literals`可能会包含新行的情况，会让你面临如何对原始字符串的第一行进行锁进的尴尬的选择。因为`protobuf`的文本是忽略空白的，所以可以通过添加一个前导`\n`来避免这个问题，但是不是所有的原始字符串的使用都如此宽容。

当遇到原始字符串中包含`)"`的时候会导致不能将`)`作为结束的分割符，这个时候一个非空的`tag`会很有用：

```cpp
std::string my_string = R"foo(This contains quoted parens "()")foo";
```

## Conclusion

`raw string literals`肯定不是我们大多数人的日常工具，但是有的时候善用这种新的语言功能会增加代码的可读性。因此下次当您试图搞清楚是否需要`\\`或者`\\\\`的时候，请尝试使用`raw string literal`替换。你的读者会感谢你，即使这样，正则表达式仍然很难。


```cpp
R"regexp((?:"(?:\\"|[^"])*"|'(?:\\'|[^'])*'))regexp"
```