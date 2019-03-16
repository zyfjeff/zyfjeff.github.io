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


## OS基础

最初的OS只能运行在16位的8088处理器上，只能访问到1MB的地址空间从0x00000000 ~ 0x000FFFFF，前640KB被称为低内存区，是OS可以直接使用的内存。
剩下的384KB从0x000A0000 ~ 0x000FFFFF则是保留的用于访问硬件的。其中BIOS占用64KB从0x000F0000 ~ 0X000FFFF、还有VGA占用0X000A0000 ~ 0x000C0000
此后Intel突破1MB内存限制后，为了向后兼容仍然保持这样的内存布局，这样就使得内存中存在一个hole，0x000A0000~0x00100000。内存则被分成了两个部分，第一个部分就是从0x00000000到
0x000A0000这640KB被称为低内存区，另外一个部分就是从0x00100000开始，被称为extended memory。此外BIOS通过会保留32位的物理地址空间的顶部内存空间，用于给32位的PCI设备使用。

启动后，第一条指定的位置是0x000ffff0，也就是顶部的16个字节的位置(为什么是这里)，第一条指令是一个ljmp的指令，跳转到0x000fe05b的位置，也是属于顶部的64KB的范围内，这个位置是固定的
也就是说，BIOS真正开始的地方就是0xfe05b处。

BIOS执行了一堆自检操作后，找到启动设备，然后载入启动设备的前512字节到0x7c00处，然后开始执行，这个时候控制权就转交给bootloader了，bootload主要做两件事:

1. 实模式到32位保护模式的切换
2. 从磁盘读取内核


如何从实模式切换到保护模式

1. 打开A20地址线后
2. 加载gdt
3. CR0寄存器的PE位置设置为1

```C++
   # Enable A20:
   #   For backwards compatibility with the earliest PCs, physical
   #   address line 20 is tied low, so that addresses higher than
   #   1MB wrap around to zero by default.  This code undoes this.
 seta20.1:
   inb     $0x64,%al               # Wait for not busy
   testb   $0x2,%al
   jnz     seta20.1

   movb    $0xd1,%al               # 0xd1 -> port 0x64
   outb    %al,$0x64

 seta20.2:
   inb     $0x64,%al               # Wait for not busy
   testb   $0x2,%al
   jnz     seta20.2

   movb    $0xdf,%al               # 0xdf -> port 0x60
   outb    %al,$0x60

   lgdt    gdtdesc
   movl    %cr0, %eax
   orl     $CR0_PE_ON, %eax
   movl    %eax, %cr0
```

Bootloader如何知道内核大小，读取指定的扇区把内核加载到对应的地方?

读取ELF头部信息，通过ELF头知道这个程序包含多少字节大小的内容，ELF头也指定了入口函数的地址，载入地址、链接地址等信息。

```c
   struct Proghdr *ph, *eph;

   // read 1st page off disk
   readseg((uint32_t) ELFHDR, SECTSIZE*8, 0);

   // is this a valid ELF?
   if (ELFHDR->e_magic != ELF_MAGIC)
     goto bad;

   // load each program segment (ignores ph flags)
   ph = (struct Proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
   eph = ph + ELFHDR->e_phnum;
   for (; ph < eph; ph++)
     // p_pa is the load address of this segment (as well
     // as the physical address)
     readseg(ph->p_pa, ph->p_memsz, ph->p_offset);

   // call the entry point from the ELF header
   // note: does not return!
   ((void (*)(void)) (ELFHDR->e_entry))();
```

ELF有一个固定的4字节的header，还有变长的program header。

```cpp
tianqian-zyf@tianqia-zyf:~/6.828/lab$ objdump -h obj/kern/kernel

obj/kern/kernel:     file format elf32-i386

Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .text         000019e9  f0100000  00100000  00001000  2**4
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  1 .rodata       000006c0  f0101a00  00101a00  00002a00  2**5
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  2 .stab         00003b95  f01020c0  001020c0  000030c0  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  3 .stabstr      00001948  f0105c55  00105c55  00006c55  2**0
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  4 .data         00009300  f0108000  00108000  00009000  2**12
                  CONTENTS, ALLOC, LOAD, DATA
  5 .got          00000008  f0111300  00111300  00012300  2**2
                  CONTENTS, ALLOC, LOAD, DATA
  6 .got.plt      0000000c  f0111308  00111308  00012308  2**2
                  CONTENTS, ALLOC, LOAD, DATA
  7 .data.rel.local 00001000  f0112000  00112000  00013000  2**12
                  CONTENTS, ALLOC, LOAD, DATA
  8 .data.rel.ro.local 00000044  f0113000  00113000  00014000  2**2
                  CONTENTS, ALLOC, LOAD, DATA
  9 .bss          00000648  f0113060  00113060  00014060  2**5
                  CONTENTS, ALLOC, LOAD, DATA
 10 .comment      0000002a  00000000  00000000  000146a8  2**0
                  CONTENTS, READONLY
```

载入内核后，内核开始进行一系列的初始化，最重要的就是开启虚拟地址。在此之前内核一直运行在物理地址下，开启虚拟地址需要开启分页，相关代码如下:

```cpp
        # We haven't set up virtual memory yet, so we're running from
        # the physical address the boot loader loaded the kernel at: 1MB
        # (plus a few bytes).  However, the C code is linked to run at
        # KERNBASE+1MB.  Hence, we set up a trivial page directory that
        # translates virtual addresses [KERNBASE, KERNBASE+4MB) to
        # physical addresses [0, 4MB).  This 4MB region will be
        # sufficient until we set up our real page table in mem_init
        # in lab 2.

        # Load the physical address of entry_pgdir into cr3.  entry_pgdir
        # is defined in entrypgdir.c.
        movl    $(RELOC(entry_pgdir)), %eax
        movl    %eax, %cr3
        # Turn on paging.
        movl    %cr0, %eax
        orl     $(CR0_PE|CR0_PG|CR0_WP), %eax
        movl    %eax, %cr0
```