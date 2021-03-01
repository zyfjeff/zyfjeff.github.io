---
hide:
  - toc        # Hide table of contents
---
# Tip of the Week #88: Initialization: =, (), and {}

> Originally posted as TotW #88 on Jan 27, 2015
> by Titus Winters (titus@google.com), on behalf of the Google C++ Style Arbiters

C++11提供了一种新的语法，被称为`统一初始化语法`，其目的是为了统一各种风格的初始化方式，从而[避免一些令人烦恼的解析](https://en.wikipedia.org/wiki/Most_vexing_parse)，以及避免窄化转换等
这个新的机制，意味着现在有了另外一种初始化的语法，并有了自己的折衷方案。

## C++11 Brace Initialization

一些统一初始化语法的支持者建议我们使用`{}`和直接初始化（不使用“=”，尽管在大多数情况下，两种形式都调用同一个构造函数）来初始化所有类型：

```cpp
int x{2};
std::string foo{"Hello World"};
std::vector<int> v{1, 2, 3};
```

和下面这种

```cpp
int x = 2;
std::string foo = "Hello World";
std::vector<int> v = {1, 2, 3};
```

这个方法有两个缺点: 首先，“统一”是一个延伸：在某些情况下，在调用什么以及如何调用中仍然存在歧义（对于普通读者，而不是编译器）。

```cpp
std::vector<std::string> strings{2}; // A vector of two empty strings.
std::vector<int> ints{2};            // A vector containing only the integer 2.
```

其次，这种语法并不完全直观，没有其他通用语言使用这种语法，这种语言当然可以引入新的、令人惊讶的语法，而且在某些情况下（特别是在泛型代码中）有必要使用这种语言的技术原因。
重要的问题是：我们应该在多大程度上改变我们的习惯和对语言的理解来利用这种改变？在改变我们的习惯或现有代码时，这些好处值得付出代价吗？对于统一的初始化语法，我们一般不认为好处大于缺点。

## Best Practices for Initialization

相反，对于“如何初始化变量”，我们建议使用以下指导原则。“，既要遵循自己的代码，也要在代码评论中引用：

* 当直接初始化字面值的时候，使用赋值语法(例如: `int`、`float`、`std::string`值时)，对于智能指针例如`std::shared_ptr`、`std::unique_ptr`以及容器(`std::vector`、 `std::map`等)时执行`struct初始化`或者是做拷贝初始化

```cpp
int x = 2;
std::string foo = "Hello World";
std::vector<int> v = {1, 2, 3};
std::unique_ptr<Matrix> matrix = NewMatrix(rows, cols);
MyStruct x = {true, 5.0};
MyProto copied_proto = original_proto;
```

替代如下代码:

```cpp
// Bad code
int x{2};
std::string foo{"Hello World"};
std::vector<int> v{1, 2, 3};
std::unique_ptr<Matrix> matrix{NewMatrix(rows, cols)};
MyStruct x{true, 5.0};
MyProto copied_proto{original_proto};
```

* 当初始化需要执行某些逻辑的时候，使用传统的构造语法(带有括号的)，而不是简单的将值组合在一起

```cpp
Frobber frobber(size, &bazzer_to_duplicate);
std::vector<double> fifty_pies(50, 3.14);
```

相比于如下代码:

```cpp
// Bad code

// Could invoke an intializer list constructor, or a two-argument constructor.
Frobber frobber{size, &bazzer_to_duplicate};

// Makes a vector of two doubles.
std::vector<double> fifty_pies{50, 3.14};
```

* 仅当上述方法无法编译的时候，才使用`{}`初始化，而不是`=`

```cpp
class Foo {
 public:
  Foo(int a, int b, int c) : array_{a, b, c} {}

 private:
  int array_[5];
  // Requires {}s because the constructor is marked explicit
  // and the type is non-copyable.
  EventManager em{EventManager::Options()};
};
```

* 用于不要将`{}`初始化和`auto`混合在一起使用

```cpp
// Bad code
auto x{1};
auto y = {2}; // This is a std::initializer_list<int>!
```

(对于语言律师来说：在可用时，首选复制初始化，而不是直接初始化；在诉诸直接初始化时，优先使用大括号而不是括号)


或许对于这个问题的描述最好的文章是来自于`Herb Sutter`的[GotW](https://herbsutter.com/2013/05/09/gotw-1-solution/)，尽管他展示的example中包含了
直接通过`{}`来初始化int类型，但是最终他的建议和我们这里的内容大致相符，其中有一个警告: `Herb`说，你更喜欢只看到`=`符号，我们毫不含糊地希望看到这一点。
结合在多参数构造函数上更一致地使用显式（参见提示[Tips 142](https://abseil.io/tips/142)），这提供了可读性，显式性和正确性之间的平衡。


## Conclusion

统一初始化语法的权衡，通常是不值得： 我们的编译器已经警告过最令人烦恼的解析(你可以使用大括号来初始化或者添加parents来解决问题)，还有窄化转换并不值得大括号带来的可读性问题(最终，我们需要有一种不同的
方案来解决窄化转换的问题)。`Style Arbiters`认为这个问题不足以制定正式的规则，特别是因为有些情况(特别是在通用代码中)窄化转换用于支撑初始化可能是合理的。