## Linux中的可执行程序有什么?

自从小的时候发现可执行文件仅仅只是一个文件的时候，可执行文件便开始让我着迷。如果你将`.exe`后缀重命名为其他名称，则可以在记事本中打开这个文件!
而且，如果将其他后缀的文件重命名为.exe后缀，则会出现一个简洁的错误框。

显然这些文件有所不同。从记事本中可以看到，它们大多是乱码，但是在那种混乱中必须保证秩序。12岁的我知道了这一点后，尽管我还不太了解如何或者在何处才能深入挖掘这一切。

因此本系列是奉献给过于的我。在本文中，我们将尝试了解Linux可执行文件的组织方式，如何执行它们，以及如何使程序从链接器中重新获得可执行文件并对其进行压缩-正因为我们可以这样做。

由于最后一个大系列[制作自己的ping](https://fasterthanli.me/blog/2019/making-our-own-ping/)完全是关于Windows的，因此本系列将重点介绍64位Linux。

### 但首先，让我编写汇编

在本系列的整个过程中，我们肯定希望最终可以打包出一个自己的可执行文件。但是，正如我们在处理以太网、IPv4和ICMP时所做的一样，我们首先要利用各种工具，对一个可以正常工作的Linux可执行文件
进行深度分析。

> Hot tip:
> ELF代表可执行和可链接格式。它于1983年作为[SysV 4](https://en.wikipedia.org/wiki/UNIX_System_V#SVR4)的一部分首次发布，尽管增加了新的部分，但它仍在今天的Linux上使用

我不得不回过头来去看[艰难地阅读文件-第2部分](https://fasterthanli.me/blog/2019/reading-files-the-hard-way-2/)，以快速了解nasm(Netwide汇编程序)，因此，如果您也需要的话，我也不会怪您。

无论如何，下面这段代码是一个简短的版本：用于在标准输出中打印"hi there"，然后加上换行符：

```asm
; in `hello.asm`

        global _start

        section .text

_start: mov rdi, 1      ; stdout fd
        mov rsi, msg
        mov rdx, 9      ; 8 chars + newline
        mov rax, 1      ; write syscall
        syscall

        xor rdi, rdi    ; return code 0
        mov rax, 60     ; exit syscall
        syscall
        
        section .data

msg:    db "hi there", 10
```

提醒一下，[Filippo的Searchable Linux Syscall表](https://filippo.io/linux-syscall-table/)非常出色。
我们可以很容易地构建和链接上面这段代码。

