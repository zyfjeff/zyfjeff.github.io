## Tip of the Week #108: Avoid std::bind

> Originally published as totw/108 on 2016-01-07
> By Roman Perepelitsa (roman.perepelitsa@gmail.com)
> Updated 2019-12-19

> Quicklink: abseil.io/tips/108

### Avoid std::bind

这个Tips总结了在写代码的时候为什么要远离`std::bind`。

正确使用`std::bind()`是很难的，让我们来看一看下面这个例子，这段代码对您来说看起来不错吗？

```cpp
void DoStuffAsync(std::function<void(Status)>; cb);

class MyClass {
  void Start() {
    DoStuffAsync(std::bind(&MyClass::OnDone, this));
  }
  void OnDone(Status status);
};
```

许多经验丰富的`C++`工程师编写了这样的代码，只是发现它无法编译。这段代码只能和`std::function<void()>`类型的function一起工作，但是上面例子中的`MyClass::OnDone`是包含了额外的参数。那该如何?

`std::bind()`并没有像许多`C++`工程师期望的那样绑定函数的前N个参数，你必须指定每一个参数，因此正确的使用`std::bind`应该是下面这种方式。

```cpp
std::bind(&MyClass::OnDone, this, std::placeholders::_1)
```

代码看起来很丑陋，那有没有更好的办法呢? 使用`absl::bind_front`替换。

```cpp
absl::bind_front(&MyClass::OnDone, this)
```

好吧，`absl::bind_front()`确实做到了：它绑定了前N个参数，并完美地转发了其余参数：`absl::bind_front(F，a，b)(x，y)`的值为`F(a，b，x ，y)`。

嗯，恢复了理智。想现在看到真正恐怖的东西吗？下面的这段代码的作用是什么？

```cpp
void DoStuffAsync(std::function<void(Status)> cb);

class MyClass {
  void Start() {
    DoStuffAsync(std::bind(&MyClass::OnDone, this));
  }
  void OnDone();  // No Status here.
};
```

`OnDone`没有参数，`DoStuffAsync()`的回调函数参数是需要有一个Status参数的。你可能预期会有一个编译错误在这里。但是实际上代码没有任何`warning`。
因为`std::bind`过分地弥合了差距。来自`DoStuffAsync()`的潜在错误会被静默忽略。

这样的代码可能会导致严重的损坏。比如某些IO操作实际上可能没有成功，但是因为上面的代码会导致始终都是成功的，这可能是毁灭性的。也许`MyClass`的作者没有意识到`DoStuffAsync()`可能会产生需要处理的错误
也许`DoStuffAsync()`曾经接受`std::function<void()>`作为参数，然后其作者决定引入错误模式并更新所有停止编译的调用方。不管哪种方式，该错误都会进入生产代码。

`std::bind()`禁用了我们依赖的基本的编译时检查之一，编译器通常会告诉你，调用者是否传递了比您期望的更多的参数，但是对于`std::bind()`则无效。够恐怖吗？

另外一个例子，你如何看待下面这段代码?

```cpp
void Process(std::unique_ptr<Request> req);

void ProcessAsync(std::unique_ptr<Request> req) {
  thread::DefaultQueue()->Add(
      ToCallback(std::bind(&MyClass::Process, this, std::move(req))));
}
```

跨异步边界传递的`std::unique_ptr`。不用说，`std::bind()`并不是解决方案，上面这段代码无法编译，因为`std::bind()`不会将绑定的仅支持移动的参数移至目标函数。
只需用`absl::bind_front()`替换`std::bind()`即可解决。

下一个示例甚至使一些`C++专家`经常绊倒。看看是否可以找到问题。

```cpp
// F must be callable without arguments.
template <class F>
void DoStuffAsync(F cb) {
  auto DoStuffAndNotify = [](F cb) {
    DoStuff();
    cb();
  };
  thread::DefaultQueue()->Schedule(std::bind(DoStuffAndNotify, cb));
}

class MyClass {
  void Start() {
    DoStuffAsync(std::bind(&yClass::OnDone, this));
  }
  void OnDone();
};
```

上面的代码是无法编译的，因为传递`std::bind()`的结果给另外一个`std::bind()`是一个特殊的case。正常情况下`std::bind(F, arg)()`最终计算为`F(arg)`。
除非arg是另一个`std::bind()`调用的结果，在这种情况下，它将求值为`F(arg())`。如果将arg可以转换为`std::function<void()>`，则会失去`std::bind`的效果。

将`std::bind()`应用于您无法控制的类型始终是一个错误，`DoStuffAsync()`不应该应用`std::bind()`到模版参数，`absl::bind_front`或`lambda`都可以正常工作。

`DoStuffAsync()`的作者甚至可能进行了完全绿色的测试，因为它们始终将lambda或std :: function作为参数传递，而从不传递std :: bind（）的结果。遇到此错误时，MyClass的作者会感到困惑。