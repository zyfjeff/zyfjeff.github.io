
* 每一个存活的实例对象都有一个唯一的ID值，但是可能会被复用，是不保证在整个进程生命周期内都是唯一的。

* dis模块可用于反汇编代码

```python

In [1]: source = """
   ...: print("hello, world!")
   ...: print(1 + 2)
   ...: """

In [2]: code = compile(source, "demo", "exec")

In [3]: import dis

In [4]: dis.show_code(code)
Name:              <module>
Filename:          demo
Argument count:    0
Kw-only arguments: 0
Number of locals:  0
Stack size:        2
Flags:             NOFREE
Constants:
   0: 'hello, world!'
   1: 3
   2: None
Names:
   0: print

In [5]: dis.dis(code)
  2           0 LOAD_NAME                0 (print)
              2 LOAD_CONST               0 ('hello, world!')
              4 CALL_FUNCTION            1
              6 POP_TOP

  3           8 LOAD_NAME                0 (print)
             10 LOAD_CONST               1 (3)
             12 CALL_FUNCTION            1
             14 POP_TOP
             16 LOAD_CONST               2 (None)
             18 RETURN_VALUE

In [6]: exec(code)
hello, world!
3
```

* py_compile模块用来编译代码，生成pyc文件、compileall模块用于编译整个目录等

* eval 用于执行单个表达式、exec则用于执行代码块，无论是eval还是exec都需要携带上下文环境，默认直接使用当前全局和本地名字空间，也可以指定名字空间

* 自定义类型可以重载`__bool__`和`__len__`来影响bool转换结果，对于数字零、空值(None)、空序列、空字典等被视作False

* Enum枚举类型

默认是数字类型

```python
In [49]: import enum

In [50]: Color = enum.Enum("Color", "BLACK YELLOW BLUE RED")

In [51]: isinstance(Color.BLACK, Color)
Out[51]: True

In [52]: list(Color)
Out[52]: [<Color.BLACK: 1>, <Color.YELLOW: 2>, <Color.BLUE: 3>, <Color.RED: 4>]

In [53]:
```

还可以通过继承来自定义多种类型

```python

```

* Python中没有指针和引用，如果需要引用数据的一部分可以使用`memoryview`

* `sys.intern` 将动态字符串池化，以便再次复用

* `+=` vs `+`的差异

```python
>>> a = [1, 2]
>>> b = a
>>> a = a + [3, 4]
>>> a
[1, 2, 3, 4]
>>> b
[1, 2]


>>> a = [1, 2]
>>> b = a
>>> a += [3, 4]
>>> a
[1, 2, 3, 4]
>>> b
[1, 2, 3, 4]
>>> a is b
True
```

* 自定义`__eq__`和`__lt__`用来实现自定义排序

