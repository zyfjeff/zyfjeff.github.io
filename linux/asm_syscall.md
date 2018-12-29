
## Linux: x86-64 syscall

The syscall number is passed in register rax. Parameters are passed in registers [rdi, rsi, rdx, rcx, r8, r9]. 
I haven’t found documentation on what x86-64 Linux does for syscalls with more than six parameters. The syscall instruction is used to pass control to the kernel.
Linux syscall numbers for x86-64 are defined in arch/x86/entry/syscalls/syscall_64.tbl.

```
.data
    .set .L_STDOUT,        1
    .set .L_SYSCALL_EXIT,  60
    .set .L_SYSCALL_WRITE, 1
    .L_message:
        .ascii "Hello, world!\n"
        .set .L_message_len, . - .L_message

.text
    .global _start
    _start:
        # write(STDOUT, message, message_len)
        mov     $.L_SYSCALL_WRITE, %rax
        mov     $.L_STDOUT,        %rdi
        mov     $.L_message,       %rsi
        mov     $.L_message_len,   %rdx
        syscall

        # exit(0)
        mov     $.L_SYSCALL_EXIT, %rax
        mov     $0,               %rdi
        syscall
```

* static linking

```
$ as --64 -o hello.o hello.s
$ ld -m elf_x86_64 -o hello hello.o
$ file hello
hello: ELF 64-bit LSB executable, x86-64, version 1 (SYSV), statically linked, not stripped
$ ./hello
Hello, world!
```


* dynamic linking

```
$ as --64 -o hello.o hello.s
$ ld -m elf_x86_64 -o hello hello.o \
   --dynamic-linker /lib64/ld-linux-x86-64.so.2 \
   -l:ld-linux-x86-64.so.2
$ file hello
hello: ELF 64-bit LSB executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, not stripped
$ ldd hello
    /lib64/ld-linux-x86-64.so.2 (0x00007f472a831000)
    linux-vdso.so.1 (0x00007ffe83d7a000)
$ ./hello
Hello, world!
```
