## Lua基础

* 字符串连接用 `..`
* 字符串长度用 `#`
* 数字字符串相加会转换为数字相加
* Lua中的表可以是hashmap、可以是关联数组，没初始化的table是nil，表的值不能是nil
* Lua中的数组索引是从1开始
* Lua中的主要工作在协程中，任意时刻只能运行一个协程，并且处于运行状态的协程只有在被挂起时才会停止。
* userdata是一种用户自定义类型，由应用程序或者C++创建的类型，可以将任意C++的任意数据类型的数据转存储在Lua变量中
* ipairs和pairs都是迭代器，其中ipars只迭代索引类型的元素，pairs则是所有
* module机制本质上就是一个module表，将需要暴露出来的方法填入module表中即可

## Lua原理

* Lua中一段可以被解释器解释执行的代码叫做Chunk，Luac将源代码翻译成二进制的Chunk，然后交由Lua来解释运行

![lua-binchunk](lua-binchunk.jpg)

* Lua编译器是以函数为单位进行编译，每一个函数会被Lua编译器编译为一个内部结构，这个结构叫做原型，
  主要包含六个部分: 函数的基本信息(参数数量、局部变量等)、字节码、常量表、Upvalue表、调试信息、子函数原型表

![lua-prototype](lua-prototype.jpg)


```lua
print("Hello World")

转换为:

function main(...)
  print("Hello World")
  return
end
```

使用两个`-l`选项可以进入详细模式，打印出常量表、局部变量表、Upvalue表等信息

`luac -l -l hello.lua`

```lua
main <hello.lua:0,0> (4 instructions at 0x7f8440c031c0)
0+ params, 2 slots, 1 upvalue, 0 locals, 2 constants, 0 functions
        1       [1]     GETTABUP        0 0 -1  ; _ENV "print"
        2       [1]     LOADK           1 -2    ; "Hello World"
        3       [1]     CALL            0 2 1
        4       [1]     RETURN          0 1
constants (2) for 0x7f8440c031c0:
        1       "print"
        2       "Hello World"
locals (0) for 0x7f8440c031c0:
upvalues (1) for 0x7f8440c031c0:
        0       _ENV    1       0
```

一个二进制chunk主要由头部、主函数中的upvalues表的数量、函数原型等三个部分组成，其中头部总共30个字节，包含了
签名、版本号、格式号、LUAC_DATA、各种数据类型占用的字节数、以及大小端、浮点数格式识别信息等。签名的值是`0x1B4C7561`，其
字面量的形式就是`\x1bLua`，版本号由三个部分组成、Major Version、Minor Verison、Release Version，只会保存
Major和Minor的版本号，因为Release号只表示bug修复，不影响二进制格式。chunk格式号默认是0，表示chunk的格式。
LUAC_DATA占六个字节，前二个字节是0x1993这是lua的发布年份、后面四个字节是0x0D、0x0A、0x1、0A。剩下的就是cint、size_t、Lua虚拟机指令、lua整数、
lua浮点数等五种数据类型的占用的字节数大小。接着会有一个LUAC_INT，其值为0x5678，

![lua-header](lua-header.jpg)




## Lua API

lua_State是对解释器状态的封装，可以在多个解释器实例之间进行切换，使用lua_newstate类创建lua_State实例

Lua中一共支持8种数据类型，分别是nil、布尔、数字、字符串、表、函数、线程、用户数据