* x86平台上，目前有两套固件标准，一个就是早期的BIOS、另外一个就是UEFI。
* 为了避免让每一个操作系统实现bootloader，所以有了bootloader标准Multiboot，这个标准定义了bootloader和OS交互的接口，其参考实现就是grub。
* [ReadZone](https://os.phil-opp.com/red-zone/)问题
* 通过内存映射的方式访问，VGA (0xb8000)
* `QEMU -serial` 可以将串口输出重定向到另外一个地方，例如: `-serial mon:stdio`、`-serial file:output-file.txt`
* x86平台下，大概有20多种不通的CPU异常类型，最为重要的有以下几个:
  1. `Page Fault` 页错误，页面还没有映射，或者往一个只读的页面写
  2. `Invalid Opcode` 无效的指令，比如在一个不支持SSE指针的CPU上，执行了SSE指令
  3. `General Protection Fault` 跨特权级访问
  4. `Double Fault` `Page Fault`的handler中发生了异常，会触发这个中断
  5. `Triple Fault` 同上

* 中断描述符表结构

```
u16	Function Pointer [0:15]	The lower bits of the pointer to the handler function. 函数指针
u16	GDT selector	Selector of a code segment in the global descriptor table. 段选择器，用来从全局段描述表中选择一个段
u16	Options	(see below)
u16	Function Pointer [16:31]	The middle bits of the pointer to the handler function.
u32	Function Pointer [32:63]	The remaining bits of the pointer to the handler function.
u32	Reserved

Options:
0-2	Interrupt Stack Table Index	0: Don't switch stacks, 1-7: Switch to the n-th stack in the Interrupt Stack Table when this handler is called.
3-7	Reserved
8	0: Interrupt Gate, 1: Trap Gate	If this bit is 0, interrupts are disabled when this handler is called.
9-11	must be one
12	must be zero
13‑14	Descriptor Privilege Level (DPL)	The minimal privilege level required for calling this handler.
15	Present
```
每一种异常都有一个预先定义的IDT索引，例如invalid opcode exception在table中就有一个索引是6，page faule异常的索引是14，当一个异常发生，CPU大致会做如下事情:
1. 将一些寄存器保存到堆栈上，包括指令指针，还有一个寄存器
2. 从IDT中读取对应的条目
3. 查看对应条目是否存在，不存在就抛double fault异常
4. 如果条目中表明这是interrupt gate，那就关中断
5. 通过GDT selector从全局描述符表中把Code Segment载入到段寄存器中
6. 跳到指定的handler函数中执行

* 中断调用约定(The Interrupt Calling Convention)
* 一些异常handler的组合会导致Double Fault

* x86-64下，TSS包含了`Privilege Stack Table`、`Interrupt Stack Table`、`I/O Map Base Address`等
