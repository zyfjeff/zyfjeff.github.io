---
hide:
  - toc        # Hide table of contents
---
# Tip of the Week #65: Putting Things in their Place
> Originally posted as totw/65 on 2013-12-12
> By Hyrum Wright (hyrum@hyrumwright.org)
> “Let me ’splain. No, there is too much. Let me sum up.” –Inigo Montoya

`C++11`中添加了一种新方式往标准容器中插入元素，那就是`emplace()`系列方法了。这些方法会直接在容器中创建对象，避免创建临时对象，然后通过拷贝或者移动对象到容器中。这对于几乎所有的对象来说避免了拷贝，更加高效。尤其是对于往标准容器中存储只能移动的对象(例如`std::unique_ptr`)就更为方便了。

## The Old Way and the New Way
让我们通过使用`vector`来存储的一个简单的例子来对比下两种方式。第一个例子是C++11之前的编码风格:

```cpp
class Foo {
 public:
  Foo(int x, int y);
  …
};

void addFoo() {
  std::vector<Foo> v1;
  v1.push_back(Foo(1, 2));
}

```
通过使用老的`push_back`方法，会导致Foo对象被构造两次，一次是临时构造一个Foo对象，然后将临时对象进行移动构造，放到容器中。

我们可以使用`C++11`引入的`emplace_back()`，这种方式只会引入一个对象的构造，是直接在`vector`容器元素所在内存上构造。正是由于`emplace`系列函数将其参数直接转发给底层对象的构造函数，因此我们可以直接提供构造函数参数，从而无需创建临时的`Foo`对象。

```cpp
void addBetterFoo() {
  std::vector<Foo> v2;
  v2.emplace_back(1, 2);
}
```

## Using Emplace Methods for Move-Only Operations
到目前为止，我们已经研究过`emplace`方法可以提高性能的情况，此外它可以让之前不能工作的代码可以正常工作，例如容器中的类型是只能被移动的类型像`std::unique_ptr`。考虑下面这段代码:

```cpp
std::vector<std::unique_ptr<Foo>> v1;
```
如何才能向这个容器中插入一个元素呢? 一种方式就是通过`push_back`直接在参数中构造对象:

```cpp
v1.push_back(std::unique_ptr<Foo>(new Foo(1, 2)));
```
这种语法有效，但可能有点笨拙。不幸的是，解决这种混乱的传统方式充满了复杂性：

```cpp
Foo *f2 = new Foo(1, 2);
v1.push_back(std::unique_ptr<Foo>(f2));
```
上面这段代码可以编译，但是它使得在被插入前，指针的所有权变的不清晰。甚至更糟糕的是，`vector`拥有了该对象，但是`f2`仍然有效，并且有可能在此后被删除。对于不知情的读者来说，这种所有权模式可能会令人困惑，特别是如果构造和插入不是如上所述的顺序事件。

其他的解决方案甚至都无法编译，因为`unique_ptr`是不能被拷贝的:

```cpp
std::unique_ptr<Foo> f(new Foo(1, 2));
v1.push_back(f);             // Does not compile!
v1.push_back(new Foo(1, 2)); // Does not compile!
```
使用`emplace`方法会使得对象的创建更为直观，如果你需要把`unique_ptr`放到`vector`容器中，可以像下面这样通过`std::move`来完成:

```cpp
std::unique_ptr<Foo> f(new Foo(1, 2));
v1.emplace_back(new Foo(1, 2));
v1.push_back(std::move(f));
```

通过把`emplace`和标准的迭代器结合，可以将对象插入到`vector`容器中的任何位置:

```cpp
v1.emplace(v1.begin(), new Foo(1, 2));
```
实际上，我们不希望看到上述构造unique_ptr的这些方法，而是希望通过`std::make_unique`(C++14)，或者是`absl::make_unique`(C++11)。

## Conclusion
本文使用`vector`来作为example中的标准容器，实际上`emplace`同样也适用于`map`、`list`以及其它的STL容器。当`unique_ptr`和`emplace`结合，使得在堆上分配的对象其所有权的语义更加清晰。希望通过本文能让您感受到新的容器方法的强大功能，以及满足在您自己的代码中适当使用它们的愿望。
