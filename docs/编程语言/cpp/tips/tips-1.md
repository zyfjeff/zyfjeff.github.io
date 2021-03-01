---
hide:
  - toc        # Hide table of contents
---
# Tip of the Week #1: string_view


> Originally published as totw/1 on 2012-04-20
> *By Michael Chastain (mec.desktop@gmail.com)*
> Updated 2017-09-18


## What’s a `string_view`, and Why Should You Care?


当我们创建一个函数并且这个函数有一个参数是用来接收常量字符串的，这时你有四个选择，其中有两个是你已经知道的，另外两个你或许还不知道：


```c++
void TakesCharStar(const char* s);             // C convention
void TakesString(const string& s);             // Old Standard C++ convention
void TakesStringView(absl::string_view s);     // Abseil C++ convention
void TakesStringView(std::string_view s);      // C++17 C++ convention
```


前两种方式最适合那种调用者提供参数和行参的类型是一致的，但是如果不一致是否需要进行转换(可能是`const char*`转换为`string`或者是`sgtring`转换为`const char*`)？


调用者需要将`string`转换为`const char*`是需要使用(高效但是不方便)`c_str()`函数:


```c++
void AlreadyHasString(const string& s) {
  TakesCharStar(s.c_str());               // explicit conversion
}
```


调用者需要将一个`const char*`转换为一个`string`的时候是不需要做任何额外事情的(这是一个好消息)，但是这会导致编译器会隐式的创建一个临时的字符串(方便但是不高效)，然后将`const char*`中的内容拷贝到这个`string`中(这是个不好的消息):


```c++
void AlreadyHasCharStar(const char* s) {
  TakesString(s); // compiler will make a copy
}
```


## What to Do?


谷歌的首选是通过`string_view`的方式来接收字符串参数，在`C++17`中可以使用`std::string_view`来替代，在那些还没有使用`C++17`的代码中可以使用`absl::string_view`来替代。


一个`string_view`类的实例可以被认为是一个对现存的字符bufer的视图，具体来说`string_view`仅仅包含了一个指针和一个长度，仅用来标识一段字符数据并不拥有这段数据，不能通过这个视图来修改对应的数据。所以对一个`string_view`的拷贝是浅拷贝，是没有字符串数据的拷贝的。


`string_view`可以隐式的从一个`const char*`和`const string&`构造出来，并且不会造成字符串数据的拷贝产生O(n)的复杂度，例如当传递一个`const string&`的时候，构造函数仅会产生O(1)的复杂度，当传递一个`const char*`的时候构造函数会自动的调用一个`strlen`函数(或者你可以传递两个参数给`string_view`)。


```c++
void AlreadyHasString(const string& s) {
  TakesStringView(s); // no explicit conversion; convenient!
}


void AlreadyHasCharStar(const char* s) {
  TakesStringView(s); // no copy; efficient!
}
```


因为`string_view`不拥有数据，所以被`string_view`所指向的字符串(类似于`const char*`)都必须有一个已知的生命周期，而且其生命周期要长于`string_view`对象自己。这意味着使用`string_view`来存储字符串通常是有问题的，你需要证明你引用的底层数据其生命周期要长于`string_view`对象自己的。


如果你的API仅仅是需要在一个单次调用中引用一个字符串数据，而且不会修改数据，那么接收一个string_view作为参数这是足够的。如果你还需要在调用后还引用这个数据，或者修复这个数据，那么你需要显示通过`string(my_string_view)`来将其转换`string`类型。


将`string_view`添加到现存的代码中并不总是正确的，如果传递给这个函数的需要的就是一个`string`或者是一个`const char*`使用`string_view`并不会很高效。最佳的方案是从工具代码开始向上开始采用`string_view`来替换，或者是从一个新项目一开始就保持一致。


## A Few Additional Notes


* 和其他字符串类型不一样的点在于`string_view`传递的时候应该按照值拷贝的方式传递，就像传递`int`和`double`类型一样，因为`string_view`就是一个小数值。
* `string_view`不需要以`NULL`结尾，因此如果这样输出是不安全的:


```c++
printf("%s\n", sv.data()); // DON’T DO THIS
```


然后，下面这样是可以的:
  ```c++
  printf("%.*s\n", static_cast<int>(sv.size()), sv.data());
  ```


* 你应该像一个`string`或者`const char*`那样来输出`string_view`


```c++
std::cout << "Took '" << s << "'";
```


* 在大多数情况下你将安全的将一个现存的接收`const string&`或`const char*`参数的函数转换为`string_view`。**我们在执行此操作时遇到的唯一危险是，如果函数的地址已被占用，则会导致构建中断，因为所产生的函数指针类型将有所不同**。
