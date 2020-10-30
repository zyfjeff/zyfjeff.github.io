

# Tips

## Enable pretty print

1. set print object on
2. ptype obj/class/struct
3. set print pretty on
4. set print vtbl on


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