## 汇编基础

栈是从高地址向低地址生长的，栈底在高地址，栈顶在低地址，rsp始终指向栈顶，每次push的时候都会导致rsp的地址减少。
GCC把每一次的函数调用都转换为一个栈帧，简单来说，每一个栈帧对应一个函数。栈帧可以理解成栈中的一块连续区域。
而rbp寄存器则是指向这块栈帧的启始位置，

```assembly
pushq   %rbp
movq    %rsp, %rbp
subq    $16, %rsp
```

在每一个函数调用的开始处我们都会发现上面类似的代码，将rbp放入堆栈中，然后用rbp保存最新的栈顶地址，可以理解rbp其实是对

![stack frame](../images/stack-frame.png)


Linux寄存器传递参数顺序
%rdi、%rsi、%rdx，%rcx、%r8、%r9


```c
#include <stdio.h>

int func1(int args1, int args2, int args3) {
  int ret = args1 + args2 + args3;
  return ret;
}

int main() {
  int args1 = 10;
  int args2 = sizeof(int);
  int args3 = sizeof(char);

  int ret = func1(args1, args2, args3);
  printf("%d\n", ret);
  return 0;
}
```


```asm
func1:
	pushq	%rbp              ; 新的栈帧，把上一个栈帧的rbp寄存器保存起来，新的栈帧要使用rbp来访问堆栈
	movq	%rsp, %rbp        ; 保存新的栈顶
	movl	%edi, -20(%rbp)   ; 获取参数，并保存在堆栈上
	movl	%esi, -24(%rbp)   ; 继续保存在堆栈上
	movl	%edx, -28(%rbp)   ; 继续保存在堆栈上
	movl	-20(%rbp), %edx   ;
	movl	-24(%rbp), %eax
	addl	%eax, %edx        ; 实现加法
	movl	-28(%rbp), %eax
	addl	%edx, %eax
	movl	%eax, -4(%rbp)    ; 返回回去
	movl	-4(%rbp), %eax
	popq	%rbp
	ret

main:
  // 保存rbp寄存器，然后使用rbp保存新的栈顶寄存器，
  pushq	%rbp
	movq	%rsp, %rbp
  // 开辟一段栈空间，用来保存局部变量
	subq	$16, %rsp
  // 通过rbp来定位到对应栈空间
	movl	$10, -4(%rbp) ; args1，存放10
	movl	$4, -8(%rbp)  ; args2, 存放4
	movl	$1, -12(%rbp) ; args3, 存放1
	movl	-12(%rbp), %edx  // 第三个参数
	movl	-8(%rbp), %ecx
	movl	-4(%rbp), %eax
	movl	%ecx, %esi  // 第二个参数
	movl	%eax, %edi  // 第一个参数
	call	func1       // 函数调用
	movl	%eax, -16(%rbp)
	movl	-16(%rbp), %eax
	movl	%eax, %esi
	movl	$.LC0, %edi
	movl	$0, %eax
	call	printf
	movl	$0, %eax
	leave
	ret
```




## 基本汇编分析

```asm
00000000000006aa <main>:
 6aa:   48 83 ec 08             sub    $0x8,%rsp
 6ae:   83 ff 02                cmp    $0x2,%edi
 6b1:   75 68                   jne    71b <main+0x71>
 // edi代表第一个参数、(rdi、rsi、rdx、rcx、r8、r9，超出6的参数的使用堆栈来传递参数，通过rsp来传递)
 if (argc != 2) {
   .....
 }
```

```asm
 71b:   48 8d 3d b2 00 00 00    lea    0xb2(%rip),%rdi        # 7d4 <_IO_stdin_used+0x4>
 722:   e8 49 fe ff ff          callq  570 <puts@plt>
 727:   b8 ff ff ff ff          mov    $0xffffffff,%eax
 72c:   eb e8                   jmp    716 <main+0x6c>

 716:   48 83 c4 08             add    $0x8,%rsp
 71a:   c3                      retq

 // lea 将rip寄存器的值加上0xb2，放到rdi寄存器中
 // rip寄存器的值等于下一条指令的地址，也就是0x722
 // 通过xxd -s 地址 -l 长度 二进制程序，可以找到指定地址的ASCII码内容

tianqian-zyf@tianqia-zyf:~/crackmes$ xxd -s 0x7d4 -l 0x40 crackme02.64
000007d4: 4e65 6564 2065 7861 6374 6c79 206f 6e65  Need exactly one
000007e4: 2061 7267 756d 656e 742e 004e 6f2c 2025   argument..No, %
000007f4: 7320 6973 206e 6f74 2063 6f72 7265 6374  s is not correct
00000804: 2e0a 0070 6173 7377 6f72 6431 0059 6573  ...password1.Yes

 // rax寄存器存放返回值，这里是有符号类型的-1
 // mov    $0xffffffff,%eax，

 if (argc != 2) {
   puts("Need exactly one argument");
   return -1;
 }
```

```asm
 6b3:   48 8b 56 08             mov    0x8(%rsi),%rdx
 6b7:   0f b6 02                movzbl (%rdx),%eax
 // al寄存器低8位进行相与，看al是否是0
 6ba:   84 c0                   test   %al,%al
 6bc:   74 3d                   je     6fb <main+0x51>
 // %rip的地址为 0x702
 // 第二个参数rsi，是format string
 6fb:   48 8d 35 0f 01 00 00    lea    0x10f(%rip),%rsi        # 811 <_IO_stdin_used+0x41>
 // 第一个参数rdi的值是1，表明有一个参数
 // 第三个参数是rdx
 // 这里mov    $0x0,%eax，表示可变参数使用xmm寄存器的个数 (xmm寄存器是用来存放浮点数的，如果可变参数中有浮点数一般就是通过xmm寄存器来存放)
 702:   bf 01 00 00 00          mov    $0x1,%edi
 707:   b8 00 00 00 00          mov    $0x0,%eax
 70c:   e8 6f fe ff ff          callq  580 <__printf_chk@plt>
 711:   b8 00 00 00 00          mov    $0x0,%eax
 716:   48 83 c4 08             add    $0x8,%rsp
 71a:   c3                      retq

tianqian-zyf@tianqia-zyf:~/crackmes$ xxd -s 0x811 -l 0x40 crackme02.64
00000811: 5965 732c 2025 7320 6973 2063 6f72 7265  Yes, %s is corre
00000821: 6374 210a 0000 0001 1b03 3b3c 0000 0006  ct!.......;<....
00000831: 0000 0038 fdff ff88 0000 0068 fdff ffb0  ...8.......h....
00000841: 0000 0078 fdff ff58 0000 0082 feff ffc8  ...x...X........
// 第一个参数rdi值是1，表明有一个参数， 第二个参数rsi是format string，第三个参数是rdx，里面存放了argv[1]
 if (argv[1][0] == 0) {
   printf("Yes, %s is correct!", argv[1]);
   return 0;
 }
```

```
 6be:   3c 6f                   cmp    $0x6f,%al
 6c0:   75 6c                   jne    72e <main+0x84>
 // rip寄存器的地址是0x735
 72e:   48 8d 35 ba 00 00 00    lea    0xba(%rip),%rsi        # 7ef <_IO_stdin_used+0x1f>
 735:   bf 01 00 00 00          mov    $0x1,%edi
 73a:   b8 00 00 00 00          mov    $0x0,%eax
 73f:   e8 3c fe ff ff          callq  580 <__printf_chk@plt>
 744:   b8 01 00 00 00          mov    $0x1,%eax
 749:   eb cb                   jmp    716 <main+0x6c>

 716:   48 83 c4 08             add    $0x8,%rsp
 71a:   c3                      retq

tianqian-zyf@tianqia-zyf:~/crackmes$ xxd -s 0x7ef -l 0x40 crackme02.64
000007ef: 4e6f 2c20 2573 2069 7320 6e6f 7420 636f  No, %s is not co
000007ff: 7272 6563 742e 0a00 7061 7373 776f 7264  rrect...password
0000080f: 3100 5965 732c 2025 7320 6973 2063 6f72  1.Yes, %s is cor
0000081f: 7265 6374 210a 0000 0001 1b03 3b3c 0000  rect!.......;<..
 if (argv[1][0] != 0x6f) {
   printf("No, %s is not correct", argv[1]);
   return 1;
 }
```

```asm
 6c2:   be 01 00 00 00          mov    $0x1,%esi
 6c7:   b8 61 00 00 00          mov    $0x61,%eax
 6cc:   b9 01 00 00 00          mov    $0x1,%ecx
 6d1:   48 8d 3d 2f 01 00 00    lea    0x12f(%rip),%rdi        # 807 <_IO_stdin_used+0x37>
 // argv[1] + 1 * 1 > argv[1][1]
 6d8:   0f b6 0c 0a             movzbl (%rdx,%rcx,1),%ecx
 6dc:   84 c9                   test   %cl,%cl
 6de:   74 1b                   je     6fb <main+0x51>

 // %rip的地址为 0x702
 6fb:   48 8d 35 0f 01 00 00    lea    0x10f(%rip),%rsi        # 811 <_IO_stdin_used+0x41>
 702:   bf 01 00 00 00          mov    $0x1,%edi
 707:   b8 00 00 00 00          mov    $0x0,%eax
 70c:   e8 6f fe ff ff          callq  580 <__printf_chk@plt>
 711:   b8 00 00 00 00          mov    $0x0,%eax
 716:   48 83 c4 08             add    $0x8,%rsp
 71a:   c3                      retq

 if (argv[1][1] == 0) {
   printf("Yes, %s is correct!", argv[1]);
   return 0;
 }
```

```asm
 6e0:   0f be c0                movsbl %al,%eax
 6e3:   83 e8 01                sub    $0x1,%eax
 6e6:   0f be c9                movsbl %cl,%ecx
 6e9:   39 c8                   cmp    %ecx,%eax
 6eb:   75 41                   jne    72e <main+0x84>
 72e:   48 8d 35 ba 00 00 00    lea    0xba(%rip),%rsi        # 7ef <_IO_stdin_used+0x1f>
 735:   bf 01 00 00 00          mov    $0x1,%edi
 73a:   b8 00 00 00 00          mov    $0x0,%eax
 73f:   e8 3c fe ff ff          callq  580 <__printf_chk@plt>
 744:   b8 01 00 00 00          mov    $0x1,%eax
 749:   eb cb                   jmp    716 <main+0x6c>

 716:   48 83 c4 08             add    $0x8,%rsp
 71a:   c3                      retq

 if (argv[1][1] != 0x60) {
   printf("No, %s is not correct", argv[1]);
   return 1;
 }
```

```asm
 6ed:   83 c6 01                add    $0x1,%esi
 6f0:   48 63 ce                movslq %esi,%rcx
 6f3:   0f b6 04 0f             movzbl (%rdi,%rcx,1),%eax
 6f7:   84 c0                   test   %al,%al
 6f9:   75 dd                   jne    6d8 <main+0x2e>

 6d8:   0f b6 0c 0a             movzbl (%rdx,%rcx,1),%ecx
 6dc:   84 c9                   test   %cl,%cl
 6de:   74 1b                   je     6fb <main+0x51>

 // %rip的地址为 0x702
 6fb:   48 8d 35 0f 01 00 00    lea    0x10f(%rip),%rsi        # 811 <_IO_stdin_used+0x41>
 702:   bf 01 00 00 00          mov    $0x1,%edi
 707:   b8 00 00 00 00          mov    $0x0,%eax
 70c:   e8 6f fe ff ff          callq  580 <__printf_chk@plt>
 711:   b8 00 00 00 00          mov    $0x0,%eax
 716:   48 83 c4 08             add    $0x8,%rsp
 71a:   c3                      retq

```


