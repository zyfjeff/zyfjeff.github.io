## Tip of the Week #120: Return Values are Untouchable

> Originally posted as TotW #120 on August 16, 2012
> by Samuel Benzaquen, (sbenza@google.com)

让我们假设你有下面这个代码片段，它依靠RAII来清理资源，看起来似乎一切都符合预期

```cpp
MyStatus DoSomething() {
  MyStatus status;
  auto log_on_error = RunWhenOutOfScope([&status] {
    if (!status.ok()) LOG(ERROR) << status;
  });
  status = DoA();
  if (!status.ok()) return status;
  status = DoB();
  if (!status.ok()) return status;
  status = DoC();
  if (!status.ok()) return status;
  return status;
}
```

对上面的代码简单重构下，将最后一行改成 `return MyStatus()`，然后代码开始停止输出错误日志。

到底怎么回事?

### Summary

return语句运行后，切勿访问（读或写）函数的返回变量，除非您非常小心地正确执行此操作，否则行为是不确定的。
返回变量在复制或移动后被析构函数隐式访问（请参见C ++ 11标准[stmt.return]第6.6.3节），这是这种意外访问的发生方式，但复制/移动可能被忽略。 这就是行为未定义的原因。

本Tips仅在返回非引用局部变量时适用。返回任何其他表达式都不会触发此问题。

### The Problem

对于return语句，有两种不同的优化可以修改我们原始代码段的行为：NRVO（请参见[TotW#11](https://abseil.io/tips/11)）和隐式移动。
之前的代码起作用了，因为正在进行copy elision，并且return语句实际上没有做任何工作，变量status已经在返回地址中构造，并且清除对象在return语句之后看到MyStatus对象的唯一实例就是status。

在之后的代码中，未使用copy elision，并且将返回的变量move到返回值中。 `RunWhenOutOfScope()`在完成移动操作后运行，并且看到从MyStatus对象移来的对象。
请注意，之前的代码也不正确，因为它依赖copy elision来确保正确性，我们鼓励您依靠copy elision来提高性能（请参见[TotW＃24](https://abseil.io/tips/24)），但不能保证正确性，
毕竟，copy elision是一个可选的优化，编译器选项或编译器实现的质量会影响是否发生这种情况。


### Solution

不要在return语句后访问return的变量。注意局部变量的析构函数可能会隐式地访问return的变量。
最简单的解决方案是将功能一分为二。一个负责所有处理工作，一个调用第一个并进行事后处理（即登录错误）。例如：

```cpp
MyStatus DoSomething() {
  MyStatus status;
  status = DoA();
  if (!status.ok()) return status;
  status = DoB();
  if (!status.ok()) return status;
  status = DoC();
  if (!status.ok()) return status;
  return status;
}
MyStatus DoSomethingAndLog() {
  MyStatus status = DoSomething();
  if (!status.ok()) LOG(ERROR) << status;
  return status;
}
```

如果仅读取值，则还可以确保通过禁用优化来处理。这里强制拷贝一个副本，此后的处理将看不到被move的值。例如。：

```cpp
MyStatus DoSomething() {
  MyStatus status_no_nrvo;
  // 'status' is a reference so NRVO and all associated optimizations
  // will be disabled.
  // The 'return status;' statements will always copy the object and Logger
  // will always see the correct value.
  MyStatus& status = status_no_nrvo;
  auto log_on_error = RunWhenOutOfScope([&status] {
    if (!status.ok()) LOG(ERROR) << status;
  });
  status = DoA();
  if (!status.ok()) return status;
  status = DoB();
  if (!status.ok()) return status;
  status = DoC();
  if (!status.ok()) return status;
  return status;
}
```

### Another example

```cpp
std::string EncodeVarInt(int i) {
  std::string out;
  StringOutputStream string_output(&out);
  CodedOutputStream coded_output(&string_output);
  coded_output.WriteVarint32(i);
  return out;
}
```

`CodedOutputStream`在析构函数中进行一些工作以修剪未使用的尾随字节。如果未发生NRVO，此函数可以在字符串上保留垃圾字节。

请注意，在这种情况下，您无法强制执行NRVO，并且禁用它的技巧也无济于事。我们必须在return语句运行之前修改返回值。

一个好的解决方案是添加一个块并将该函数限制为仅在该块完成后才返回。像这样:

```cpp
std::string EncodeVarInt(int i) {
  std::string out;
  {
    StringOutputStream string_output(&out);
    CodedOutputStream coded_output(&string_output);
    coded_output.WriteVarint32(i);
  }
  // At this point the streams are destroyed and they already flushed.
  // We can safely return 'out'.
  return out;
}
```

### Conclusion

不要对将要返回的局部变量持有引用。

您无法控制是否发生NRVO。编译器的版本和选项可以从下面更改此内容。不要依赖于它的正确性。
您无法控制返回的局部变量是否触发隐式移动。您使用的类型可能会在将来进行更新以支持移动操作，此外，未来的语言标准将在更多情况下采用隐式移动，因此您不能以为只是因为现在不发生这种情况就不会在将来发生。