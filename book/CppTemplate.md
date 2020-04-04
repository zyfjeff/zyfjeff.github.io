# C++ Templates 读书笔记


* `auto`类型推导会忽略掉引用和CV限制符(`const`、`voliate`等)

```C++
int i = 42;
int const& ir = i;
auto a = ir;  // a 是int类型
```

* `std::decltype`推导出来的类型会包含CV限制符和引用，通过`std::decay`可以去掉这些CV限制符和引用


* `std::common_type`可以得到多个类型的common type

可以通过`std::decltype`结合`?`号操作符实现`std::common_type`的效果


```C++
template<typename T1, typename T2,
         typename RT = std::decay_t<std::decltype(true ? T1() : t2())>>
RT max(T1 a, T2 a) {
  return b < a ? a : b;
}
```

但是存在一个问题就是T1和T2需要有构造含糊