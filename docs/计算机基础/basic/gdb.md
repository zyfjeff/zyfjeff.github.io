---
hide:
  - navigation # Hide navigation
---

# GDB Tips




## Install pretty script

1. 下载对应gcc版本的pretty script

  *  `/opt/rh/devtoolset-8/root/usr/share/gdb/python/libstdcxx/v6/`
  * `/usr/share/gcc-4.8.5/python/libstdcxx/v6/`

> python module路径是从libstdcxx/v6/开始的，因此从这里开始要构建好完整的目录

2. 创建.gdbinit文件

```python
python
import sys
sys.path.insert(0, '/root/python')
from libstdcxx.v6.printers import register_libstdcxx_printers
register_libstdcxx_printers (None)
end
```

## 使用Gdb来dump内存信息

```
sudo gdb
> attach 进程PID
> i proc mappings  # 查看进程的地址空间信息 等同于 /proc/PID/maps
> i files  # 可以看到更为详细的内存信息 等同于 /proc/PID/smaps
> dump memory 要存放的位置 SRC DST # 把SRC到DST 地址范围内的内存信息dump出来

对dump出来的内容实用strings即可查看到该段内存中存放的字符串信息了
```
## Enable pretty print

1. set print object on
2. ptype obj/class/struct
3. set print pretty on
4. set print vtbl on
5. set print elements 0 输出全部元素
6. handle SIGPIPE nostop
7. info vbtl VAR 查看虚表

## x command
x command
Displays the memory contents at a given address using the specified format.

Syntax

```
x [Address expression]
x /[Format] [Address expression]
x /[Length][Format] [Address expression]
x
```

Parameters
Address expression
Specifies the memory address which contents will be displayed. This can be the address itself or any C/C++ expression evaluating to address.
The expression can include registers (e.g. $eip) and pseudoregisters (e.g. $pc). If the address expression is not specified,
the command will continue displaying memory contents from the address where the previous instance of this command has finished.

Format
If specified, allows overriding the output format used by the command. Valid format specifiers are:
o - octal
x - hexadecimal
d - decimal
u - unsigned decimal
t - binary
f - floating point
a - address
c - char
s - string
i - instruction
The following size modifiers are supported:

b - byte
h - halfword (16-bit value)
w - word (32-bit value)
g - giant word (64-bit value)


Length
Specifies the number of elements that will be displayed by this command.

## gdb history
set history save on
set history size 10000
set history filename ~/.gdb_history

## Dump memory