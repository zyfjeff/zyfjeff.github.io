## Tip of the Week #42: Prefer Factory Functions to Initializer Methods

> Originally posted as totw/42 on 2013-05-10
> By Geoffrey Romer [(gromer@google.com)](mailto:gromer@google.com)
> Revised 2017-12-21
> *“The man who builds a factory builds a temple; the man who works there worships there, and to each is due, not scorn and blame, but reverence and praise.” – Calvin Coolidge*

在不允许异常的环境中(例如在Google内部)，有效的`C++`构造函数必须成功，因为它没有办法把错误报告给它的调用者。当然你可以使用`abort()`，但是这样做会导致整个程序`crash`。这在生产环境的代码中通常是不被接受的。

如果你的类初始化逻辑无法避免失败的可能，一个常见的办法就是给这个类提供一个`initializer`方法(也被称为"init method")，用来执行任何可能会失败的初始化工作，并通过返回值来通知调用者初始化失败。假设用户通常在构造完成后会立即调用这个初始化方法，如果失败了就立即销毁该对象，然而这个假设并不总是记录在案，也不总是强制被约束要执行的。用户很容易会在初始化之前就开始调用其它方法，或者在初始化失败后调用。有的时候类实际上是鼓励这种行为的，例如通过提供一个在初始化之前用于配置对象的方法，又或者能够在初始化失败后读取到错误。

这个设计会让你至少维护一个至少有两个不同用户可见状态的类，通常有三个: 初始化、未初始化和初始化失败。这样的设计工作需要很多规则要遵守，这个类的每一个方法都需要指定可以在哪些状态下被调用，用户必须遵守这些规则，如果这个规则失效了，那么客户端开发人员会编写出任何可能发生的代码，无论你打算如何去支持。当这种情况发生了，那么可维护性变得很差，因为你的实现必须开始支持任何可能的初始化方法调用的组合。实际上你的这些实现已经变成了你的接口的一部分了 ([Hyrum’s Law](https://hyrumslaw.com/))。

幸运的是，有一个简单的方案并且没有这些缺点：提供一个工厂函数，它可以创建并初始化类的实例，并返回这个类的指针或`absl::optional`( [TotW #123](https://abseil.io/tips/123))，使用`null`来表明失败。这是一个使用`unique_ptr<>`的玩具示例：

```c++
// foo.h
class Foo {
 public:
  // Factory method: creates and returns a Foo.
  // May return null on failure.
  static std::unique_ptr<Foo> Create();

  // Foo is not copyable.
  Foo(const Foo&) = delete;
  Foo& operator=(const Foo&) = delete;

 private:
  // Clients can't invoke the constructor directly.
  Foo();
};

// foo.c
std::unique_ptr<Foo> Foo::Create() {
  // Note that since Foo's constructor is private, we have to use new.
  return absl::WrapUnique(new Foo());
}
```

在许多情况下，这种模式是两全其美的，工厂函数`Foo::Create`像构造函数一样，暴露出一个已经完全初始化好的对象，和构造函数不同的是，这种方式可以像调用初始化方法一样在失败的时候可以知道。工厂函数的另外一个优点就是可以返回任何子类的实例(如果是使用`absl::optional`作为返回类型的话，这是不可能的。)这允许你在不更新用户代码的情况下更换另外一种实现，甚至是根据用户的输入动态的选择实现类。

这种方法的主要缺点就是它返回的是一个指向堆上分配的对象，所以不适合那种设计在堆栈上的值类型。这种类通常不需要复杂的初始化。另外工厂函数无法用做一个派生类的构造函数需要调用父类的初始化函数进行初始化的场景，因为这些初始化方法通常在父类中都是`protected`类型的。尽管如此公共`API`仍然可以使用工厂函数。