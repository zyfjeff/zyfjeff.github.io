## Tip of the Week #101: Return Values, References, and Lifetimes

> Originally posted as totw/101 on 2015-07-29
> By Titus Winters (titus@google.com)

考虑下面的代码片段:

```cpp
const string& name = obj.GetName();
std::unique_ptr<Consumer> consumer(new Consumer(name));
```

特别是，我想引起您对`＆`的注意。它在这里合适吗?我们应该检查什么?有什么问题吗? 我发现相当数量的`C++`程序员对引用并不完全清楚，但通常都知道他们“避免制作副本”。与大多数`C++`问题一样，它要复杂得多。


### Case by Case: What is Being Returned and How is it Being Stored?

这里有两个或者三个很重要的问题:

1. 例子中`GetName()`返回的类型是什么?
2. 例子中name是用什么类型存储或者初始化?
3. 如果我们要返回引用，那么返回的对象被引用是否存在生命周期限制?


我们将继续使用String类型作为我们的示例类型，但是对于大多数non-trivial的值类型，同样也适用。

1. 返回`string`，初始化`string`，这个通常会由[RVO](https://en.wikipedia.org/wiki/Return_value_optimization)来保证，最坏情况下会使用C++11中的move(具体细节见[TotW 77](https://abseil.io/tips/77))

2. 返回`string&`或者`const string&`，初始化`string`，这会发生拷贝(因为一旦初始化新的字符串，那么他就有两个名字了，这发生了拷贝，具体细节见[TotW 77](https://abseil.io/tips/77)).有的时候这是有价值的，就像你是否需要让`string`的生命周期更长久。

3. 返回`string`，初始化`string&`，这是没办法编译的，因为你不能对一个临时变量进行引用的绑定

4. 返回`const string&`，初始化`string&`，这是没办法编译的，没办法通过这种删除去除`const`限制

5. 返回`const string&`，初始化`const string&`，这是没有性能损耗的(你仅仅是高效的返回了一个指针)。但是你已经继承了所有生命周期的限制，引用的对象生命周期多长? 大多数访问器方法返回一个对成员的引用，这个引用的生命周期和这个包含成员的对象的生命周期一样长。在这个对象的生命周期内都是有效的。

6. 返回`string&`，初始化`string&`，这个#5是一样的，但需要注意的是：返回的引用是非常量的，因此对引用的任何修改都将反映在源代码中。

7. 返回`string&`，初始化`const string&`，和`#5`是一样的。

8. 返回`string`，初始化`const string&`，在`#3`这是没办法工作的，然而`C++`语言对于这种初始化`const`引用是有特殊支持的，如果你用临时的`T`来初始化`const T&`，这是可行的，临时的`T`将不会被析构，直到对他的引用离开其作用域。

方案`#8`是使用引用的最灵活方式(不想复制，只是想赋值给一个引用而不用考虑返回的是什么)，然而因为方案`#1`，它也没有真正的为你做任何事情，它可能也不需要进行拷贝。此外，现在您的代码阅读者必须与您的局部变量类型为`const string＆`而不是`string`进行竞争，而且还担心字符串底层的数据是否超出范围或已更改。


换句话说，当review原始的代码片段时，我不得不担心:

* `GetName`返回的是值还是引用?
* Consumer的构造器是使用`string`，`const string&`，还是 `string_view`?
* 构造函数是否对于参数的生命周期有要求?(如果不仅仅是字符串)

然后，如果仅仅将示例代码中的`name`声明为`string`，它通常效率不低（由于RVO和移动语义），并且至少在对象生命周期方面是安全的。

此外，如果有对象的生命周期问题，它往往是简单的字符串存储时才会发现：尔不是看`GetName()`返回的引用的生命周期和`SetName`对于生命周期的需求之间的相关关系。你自己拥有的字符串意味着只能看本地代码和`setName()`的实现即可。

所有这些都是说：避免复制是可以的，只要您不使事情变得更复杂即可。首先在没有副本时，使代码变得更加复杂并不是一个很好的权衡。