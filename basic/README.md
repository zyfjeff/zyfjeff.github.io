* x86平台上，目前有两套固件标准，一个就是早期的BIOS、另外一个就是UEFI。
* 为了避免让每一个操作系统实现bootloader，所以有了bootloader标准Multiboot，这个标准定义了bootloader和OS交互的接口，其参考实现就是grub。
* 通过内存映射的方式访问，VGA (0xb8000)
* `QEMU -serial` 可以将串口输出重定向到另外一个地方，例如: `-serial mon:stdio`、`-serial file:output-file.txt`

## Red Zone问题

Read Zone是System V ABI的一个优化，允许函数临时去使用栈帧下的128个字节，而不用去调整堆栈指针。
这个红色区域（red zone）就是一个优化。因为这个区域不会被信号或者中断侵占，函数可以在不移动栈指针的情况下使用它存取一些临时数据——于是两个移动rsp的指令就被节省下来了。
然而，标准只说了不会被信号和终端处理程序侵占，red zone还是会被接下来的函数调用使用的，这也是为什么大多数情况下都是叶子函数（不会再调用别的函数）使用这种优化。下面我举一个例子：

![read-zone](../images/read-zone.svg)


```C++
long test2(long a, long b, long c)	/* 叶子函数 */ {
	return a*b + c;
}

long test1(long a, long b) {
	return test2(b, a, 3);
}

int main(int argc, char const *argv[]) {
	return test1(1, 2);
}
```

对应的汇编如下:

```C++
00000000004004d6 <test2>:
  4004d6:	55                   	push   %rbp
  4004d7:	48 89 e5             	mov    %rsp,%rbp
  4004da:	48 89 7d f8          	mov    %rdi,-0x8(%rbp)  // 不需要调整栈顶，直接使用原来栈顶的底部128个字节
  4004de:	48 89 75 f0          	mov    %rsi,-0x10(%rbp)
  4004e2:	48 89 55 e8          	mov    %rdx,-0x18(%rbp)
  4004e6:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
  4004ea:	48 0f af 45 f0       	imul   -0x10(%rbp),%rax
  4004ef:	48 89 c2             	mov    %rax,%rdx
  4004f2:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
  4004f6:	48 01 d0             	add    %rdx,%rax
  4004f9:	5d                   	pop    %rbp
  4004fa:	c3                   	retq

00000000004004fb <test1>:
  4004fb:	55                   	push   %rbp
  4004fc:	48 89 e5             	mov    %rsp,%rbp
  4004ff:	48 83 ec 10          	sub    $0x10,%rsp   // 调整栈顶
  400503:	48 89 7d f8          	mov    %rdi,-0x8(%rbp)
  400507:	48 89 75 f0          	mov    %rsi,-0x10(%rbp)
  40050b:	48 8b 4d f8          	mov    -0x8(%rbp),%rcx
  40050f:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
  400513:	ba 03 00 00 00       	mov    $0x3,%edx
  400518:	48 89 ce             	mov    %rcx,%rsi
  40051b:	48 89 c7             	mov    %rax,%rdi
  40051e:	e8 b3 ff ff ff       	callq  4004d6 <test2>
  400523:	c9                   	leaveq            // 恢复栈顶
  400524:	c3                   	retq

```

可以看到test1移动了栈顶指针来获取栈帧空间，即`sub $xxx, %rsp + leaveq`的组合。但是test2并没有移动栈顶指针，
而是直接使用ebp/esp（此时它们两个相等，由于是叶子也不用考虑内存对齐的问题）存放要使用的数据。


这个优化对于异常和硬件中断则会产生巨大问题，一般内部都会关掉这个优化。

![read-zone-override](../images/read-zone-overwrite.svg)
* https://os.phil-opp.com/red-zone/


## 虚拟地址映射

在开启分页的情况下，内核如何直接访问物理地址，因此我们需要有一套机制将虚拟地址映射到对应的物理地址上，所以就有了一些技术方案。

1. Identity Mapping

  虚拟地址和物理地址一一映射，带来的问题就是会导致内存碎片(外部)，和分段机制的缺点是一样的，导致在需要一片连续内存的时候无法被满足。
  而且超过frame范围的虚拟地址空间没有办法被映射。

  ![Identity Mapping](img/identity-mapped-page-tables.svg)

2. Map at a Fixed Offset

  offset+虚拟地址空间地址来一一映射到frame，这样可以避免Identity Mapping因为超过frame范围的虚拟地址没有办法进行映射的问题，但是缺点就是
  每次创建一个页表就需要创建这样的一个映射关系。而且它不允许访问其他地址空间的页表，这在创建新进程时很有用。

  ![Map at a Fixed Offset](img/page-tables-mapped-at-offset.svg)

3. Map the Complete Physical Memory

  在Identify Maaping的基础上添加offset将虚拟地址映射到全部物理内存中，这个方法可以使得内核可以访问任意的物理内存。但是需要分配很多页表
  无论是已经映射的虚拟地址还是没有映射的。这些页表会占用比较多的内存。不过我们可以通过huge page的方式来减少需要存储的页表大小。

  ![map-complete-physical-memory](img/map-complete-physical-memory.svg)

4. Temporary Mapping

  ![Temporary Mapping](img/temporarily-mapped-page-tables.svg)


5. Recursive Page Tables

  ![Recursive Page Tables](img/recursive-page-table.svg)



## SIMD和OS

SIMD(Single Instruction Multiple Data)，单条指令可以操作多个字节的数据。目前x86支持三种SIMD的标准:

1. MMX  Multi Media Extension ，这个标准定义了8个64位的寄存器，分别是mm0、mm1、mm2....mm7
2. SSE  Streaming SIMD Extensions，添加了十六个新的寄存器，分别是xmm0...xmm15，每一个寄存器都是128位
3. AVX  Advanced Vector Extensions，新增了16个256位的寄存器 ymm0....ymm15

SIMD指令虽然可以提高性能，但是对于OS来说会导致上下文切换的开销变大，每一次上下文切换或者硬件中断都需要进行上下文的保存和恢复
因为使用了SIMD指令会导致每次需要保存的内容更多，带来性能的上的损耗。所以内核开发会关闭SIMD。但是目前x86_64架构一般都会使用
SIMDl来实现浮点数的操作，如果关闭SIMD会导致错误。幸运的是LLVM给我们提供了soft-float能力，可以通过软件函数来模拟所有的浮点数操作。


## Preserved and Sscratch Registers

1. preserved registers

这类寄存器在跨函数调用的时候必须不能被改变，因此被调用者需要一开始就保存这些寄存器，然后在调用结束的时候进行恢复

rbp, rbx, rsp, r12, r13, r14, r15

callee-saved  被调用者保存

2. scratch registers

这类寄存器属于临时寄存器，被调用者在使用上是没有限制的，调用者如果向保持这些寄存器的值不变就需要保存这些寄存器。
所以这类寄存器被称为调用者保存

rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11

caller-saved  调用者保存


## Interrupt Calling Convention and Interrupt Stack Frame

1. Interrupt Calling Convention

中断调用和函数调用很类似，都有自己的调用规范，因为异常可以发生在任何地方，因此我们没办法提前保存需要的寄存器，因此
中断调用的时候需要保存所有的寄存器。中断调用结束后恢复所有保存的寄存器。为了效率，并不会保存所有的寄存器，而这是保存
被函数覆盖的寄存器。

2. Interrupt Stack Frame

  1. Aligning the stack pointer
  2. Switching stacks
  3. Pushing the old stack pointer
  4. ushing and updating the RFLAGS register
  5. Pushing the instruction pointer
  6. Pushing an error code
  7. Invoking the interrupt handler

## VGA Buffer


## IST and TSS

The Interrupt Stack Table (IST) is part of an old legacy structure called Task State Segment (TSS).
The TSS used to hold various information (e.g. processor register state) about a task in 32-bit mode
and was for example used for hardware context switching. However, hardware context switching is no longer
supported in 64-bit mode and the format of the TSS changed completely.

On x86_64, the TSS no longer holds any task specific information at all. Instead,
it holds two stack tables (the IST is one of them). The only common field between the 32-bit and 64-bit TSS is the pointer to the I/O port permissions bitmap.

The Privilege Stack Table is used by the CPU when the privilege level changes. For example,
if an exception occurs while the CPU is in user mode (privilege level 3), the CPU normally switches to kernel mode (privilege level 0)
before invoking the exception handler. In that case, the CPU would switch to the 0th stack in the Privilege
Stack Table (since 0 is the target privilege level). We don't have any user mode programs yet, so we ignore this table for now.

## The Interrupt Descriptor Table

In order to catch and handle exceptions, we have to set up a so-called Interrupt Descriptor Table (IDT).
In this table we can specify a handler function for each CPU exception. The hardware uses this table directly,

## The Global Descriptor Table

The Global Descriptor Table (GDT) is a relict that was used for memory segmentation before paging became the de facto standard.
It is still needed in 64-bit mode for various things such as kernel/user mode configuration or TSS loading.

The GDT is a structure that contains the segments of the program. It was used on older architectures to isolate programs from each other,
before paging became the standard. For more information about segmentation check out the equally named chapter of the free “Three Easy Pieces” book.
While segmentation is no longer supported in 64-bit mode, the GDT still exists. It is mostly used for two things: Switching between kernel space and user space,
and loading a TSS structure.


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


## 如何获取调用链?


## 网络基础

1. ARP和GARP

ARP协议工作在二层，用于获取IP地址对应的mac地址，报文的格式为`who has [IP_B], tell [IP_A]`，包含了要查询的IP地址，以及自己的IP地址和Mac地址信息
但是如果要查询的地址就是自己的地址，这个时候称为是GARP(Gratuitous ARP)，也就是免费的ARP，这种报文是允许的，有两个作用。

  * 用来探测IP冲突的问题，如果真的有人响应这个报文，说明局域网中有和自己相同IP的机器存在
  * 用来更新自己的Mac地址信息，当自己的Mac地址发生变化的时候，可以通过这种报文主动通知网络中的其他主机更新自己的ARP缓存



## Debuger、Compiler、ELF

通过编译器的`-g`参数可以产生debug信息，最为重要的是`debug_line`、`debug_info`两类，前者用来提供行号信息，后者就是著名的`DWARF`提供调试信息。
`debug_line`的格式如下:


```
.debug_line: line number info for a single cu
Source lines (from CU-DIE at .debug_info offset 0x0000000b):

            NS new statement, BB new basic block, ET end of text sequence
            PE prologue end, EB epilogue begin
            IS=val ISA number, DI=val discriminator value
<pc>        [lno,col] NS BB ET PE EB IS= DI= uri: "filepath"
0x004004c0  [   1, 0] NS uri: "/home/tianqian-zyf/debuger/first.cc"
0x004004cb  [   2,10] NS PE
0x004004d3  [   3,10] NS
0x004004db  [   4,14] NS
0x004004df  [   4,16]
0x004004e3  [   4,10]
0x004004e7  [   5, 7] NS
0x004004ef  [   6, 5] NS
0x004004f6  [   6, 5] NS ET
```

包含了一些说明信息，已经地址和行号的对应关系:

* NS表示一个新的语句
* BB 表示一个基本的block
* ET 表示一个编译单元的结束
* PE 是一个函数的开始


* [dwarfdump](https://www.prevanders.net/dwarf.html#releases)