## Tip of the Week #112: emplace vs. push_back

> Originally posted as totw/112 on 2016-02-25
> By Geoff Romer (gromer@google.com)
> Revised 2017-08-30
> “The less we use our power, the greater it will be.” — Thomas Jefferson

你或许已经知道(如果不知道请看[TotW65](https://abseil.io/tips/65))，C++11引入了一个强大的方式来往容器中插入一个元素，那就是`emplace`方法
这个方法可以让你在容器中使用对象的任何一个构造函数进行原地构造，包括move和copy构造。因此事实证明，只要可以使用`push`或`insert`方法，就可以使用`emplace`方法来替换，而无需进行其他的更改

```cpp
std::vector<string> my_vec;
my_vec.push_back("foo");     // This is OK, so...
my_vec.emplace_back("foo");  // This is also OK, and has the same result

std::set<string> my_set;
my_set.insert("foo");        // Same here: any insert call can be
my_set.emplace("foo");       // rewritten as an emplace call.
```

这里提出了一个很明显的问题，你应该使用哪一个方法?，或许我们应该仅仅丢弃对`push_back`和`insert`的使用，而选择一直使用`emplace`.

让我再问一个问题来回答这个问题：这两行代码做什么？

```cpp
vec1.push_back(1<<20);
vec2.emplace_back(1<<20);
```

第一行很简单，它将数字1048576添加到`vector`的末尾，但是第二行不是清楚，也不知道`vector`的类型，我们不知道会调用哪个构造函数，因此我们无法知道
第二行代码在做什么.例如: 如果`vec2`是`std::vector<int>`，那么第二行就是将数字1048576添加到`vector`的末尾，和第一行是一致的。但是如果`vec2`
是`std::vector<std::vector<int>>`，那么第二行代码会构造一个超过一百行个元素的`vector`。在次过程中会分配几兆字节的内存。

因此如果相同参数的情况下你在`push_back()`和`emplace_back()`之间做选择的话，选择`push_back()`会使得代码更易读。因为`push_back()`更具体的表达了您的意图
选择`push_back()`也更安全，假设你有一个`std::vector<std::vector<int>>`，并且你想往第一个`vector`的末尾添加一个数字。但是你一不小心忘记了下标。
如果您编写`my_vec.push_back(2 << 20)`，则会出现编译错误，并迅速发现问题。另一方面，如果您编写`my_vec.emplace_back(2 << 20)`，则代码将编译，并且直到运行时您都不会发现任何问题。

现在当涉及隐式转换的时候，`emplace_back()`可能比`push_back()`快一点。例如，我们代码中这样写`my_vec.push_back("foo")`,这行代码会首先将"foo"字符串隐式转换为临时的`string`，
然后move到容器中。然后`my_vec.emplace_back("foo")`只是直接在容器中构建`std::string`，避免了额外的move，对于一些进行move代价比较大的类型，或许是选择使用`emplace_back`替代`push_back`的原因。
尽管具有可读性和安全性成本，但随后可能不会。通常情况下，性能差异并不重要，与往常一样，经验法则是，应避免使代码不那么安全或不太清晰的“优化”，除非性能好处足以在应用程序基准测试中显示出来。

因此，通常，如果`push_back()`和`emplace_back()`都可以使用相同的参数，则您应该更喜欢`push_back()`，对于`insert()`和`emplace()`同样如此。