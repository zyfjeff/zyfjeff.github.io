## Tip of the Week #103: Flags Are Globals

> by Matt Armstrong
> Define flags at global scope in a .cc file. Declare them at most once in a corresponding .h file.

### Why Declare Things In Header Files?

使用头文件对于我们大多数人来说是一种反射，因此我们可能已经忘记了为什么使用它们:

1. 在头文件中声明一些东西，让`#include`可以在其他任何地方轻松的使用这个头文件，可以让整个程序可以看到相同的声明。
2. 在`.cc`文件中包含这个头文件，并对其声明的实体进行定义，可确保定义与声明匹配。
3. 头文件充当软件包公共API的文档。除了软件包的公共API外，不能使用其他任何形式。
4. 包含头文件，而不是重新声明实体，可以帮助工具和人员进行依赖关系分析。

### Abseil Flags Are As Vulnerable As Any Other Global

您可以正确执行此操作，而不会出现链接时错误。首先，将以下内容放在`.cc`文件中：

```cpp
// Defining --my_flag in a .cc file.
ABSL_FLAG(std::string, my_flag, "", "My flag is a string.");
```

下面代码错误地在另一个`.cc`文件（可能是测试）中声明该标志：

```cpp
// Declared in error: type should be std::string.
extern absl::Flag<int64> FLAGS_my_flag;
```

这个程序是存在问题的，发生的任何事情都是未定义行为的结果。在我的测试程序中，访问该标志时，此代码已编译、链接和奔溃了。

### Recommendations

与全局变量一样，使用命令行标志进行设计。

1. 如果可以的话，避免使用flag，具体细节参见 [TotW 45](http://abseil.io/tips/45).
2. 如果使用flag使测试更容易编写，而又不想在生产环境中进行设置，请考虑将仅测试的API添加到类中。
3. 考虑将flag视为私有静态变量。如果其他软件包需要访问它们，则将它们包装在函数中。
4. 在flag范围冲突时，在全局范围（而不是在名称空间内）定义标志，以获取链接错误。
5. 如果在多个文件中访问一个flag，则在与其定义相对应的一个.h文件中声明它。
6. 使用`ABSL_FLAG(type, ...)`宏来定义flag

### In Conclusion

Flag是全局变量，明智地使用它们。与使用其他任何全局变量一样，使用并声明它们。