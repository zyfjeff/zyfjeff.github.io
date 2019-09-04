# 如何在Linux环境下实现一个调试器

写过Linux C/C++的同学应该没有哪个没有用过GDB/LLDB吧，这两个应该算是类unix系统下最为著名的两个调试器，调试器对于大多数的人来说是一个神秘的东西，
这是因为它们的实现原理并不是很容易就能够掌握，而本文则希望给大家普及一下调试器的实现原理。

## `ptrace`

Linux下要实现一个调试器最为核心的就是`ptrace`系统调用，通过这个系统调用可以控制待调试的进程，可以获取其内存和寄存器等信息，也可以修改其内存。下面是
man手册中对于`ptrace`的一个介绍。

```
       The ptrace() system call provides a means by which one process (the
       "tracer") may observe and control the execution of another process
       (the "tracee"), and examine and change the tracee's memory and
       registers.  It is primarily used to implement breakpoint debugging
       and system call tracing.
```

通过man手册的介绍可以知道`ptrace`是实现debugger的主要工具。有了`ptrace`后就需要找到待调试的进程，这个进程有两类，一类是跟调试器(debugger)本身没有任何关系的
进程，对这类进程可以通过ptrace的`PTRACE_ATTACH`或者是`PTRACE_SEIZE`选项来进行attach，对应到gdb的`attach`命令。如果这个待调试进程是调试器的子进程那么可以
直接在子进程中执行`PTRACE_ME`来让子进程停止，并进入调试状态。

```cpp
    auto pid = fork();
    if (pid == 0) {
      // 让子进程进入调试状态
      ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
      int err = execl(prog, prog, nullptr);
      if (err == -1) {
        perror("execl failure:");
      }
      return err;
        //we're in the child process
        //execute debugee

    }
    else if (pid >= 1)  {
        //we're in the parent process
        //execute debugger
    }
}
```

通过`PTRACE_ME`选项可以使得子进程在启动后会收到SIGTRAP信号，这个信号可以使得子进程停止，这个时候我们需要在父进程(debugger)也就是调试器进程中通过waitpid来等待子进程到达
停止的这个状态。因为父子进程的执行顺序是不一定的。

```cpp
  int wait_status;
  auto options = 0;
  waitpid(pid_, &wait_status, options);

  if (WIFEXITED(wait_status)) {
     printf("exited, status=%d\n", WEXITSTATUS(wait_status));
  } else if (WIFSIGNALED(wait_status)) {
     printf("killed by signal %d\n", WTERMSIG(wait_status));
  } else if (WIFSTOPPED(wait_status)) {
     printf("stopped by signal %d\n", WSTOPSIG(wait_status));
  } else if (WIFCONTINUED(wait_status)) {
     printf("continued\n");
  }
  char *line = nullptr;
```

通过在父进程中添加waitpid，并打印子进程状态改变的原因，会输出`stopped by signal 5`，信号5就是`SIGTRAP`。到了这里我们已经可以开始调试进程了。


## `int3`

待调试的进程已经停止了，接下来就到了调试器要做的最重要的一件事了，那就是打断点。让程序运行到断点处就停止。这需要借助神奇的`int3`指令了，x86处理器下，当执行的
指令是`int3`的时候就会触发中断，Linux OS会设置好对应中断的中断处理函数，对于`int3`触发的中断来说就是向发生`int3`指令的进程发送一个`SIGTRAP`信号。通过上文
我们可以知道这个信号可以使得进程停止。

```cpp
  auto data = ptrace(PTRACE_PEEKDATA, pid_, addr_, nullptr);
  saved_data_ = static_cast<uint8_t>(data & 0xff);
  uint64_t int3 = 0xcc;
  uint64_t data_with_int3 = ((data & ~0xff) | int3);
  int ret = ptrace(PTRACE_POKEDATA, pid_, addr_, data_with_int3);
  if (ret == -1) {
    perror("ptrace pokedata for address failure");
  }
```

上面的代码中通过`PTRACE_PEEKDATA`获取要打断点的地址里面存放的值，然后修改其值，把`int3`指令放进去，当执行到这段代码就会自动执行`int3`指令了，因为这个指令只有一个字节
所以只需要修改前8位即可。修改完成后，需要恢复进程，让进程继续运行，直到运行到打断点的位置就会自动停止了。

```
  ptrace(PTRACE_CONT, pid_, nullptr, nullptr);
```

## 单步执行

有了断点后，下一步就是单步执行了，单步执行理论上可以借助多个断点来实现，但是很显然这并不优雅，幸运的是ptrace提供了`PTRACE_SINGLESTEP`来完成。

```cpp
```

## SIGTRAP信号处理

通过上文的介绍我们知道了SIGTRAP信号的重要性，它可以使得进程停止，但是停止的原因有很多，比如上文中提到的触发断点，也有可能是单步执行导致的、或者是

## 运行状态分析

除了打断点外，调试器最为重要的一个能力就是查看寄存器、打印调用堆栈、查看内存中的值等功能，也就是进程的运行状态。这些能力都可以借助ptrace来完成(这个系统调用就像一个大杂烩一样，包含了很多功能
每一类功能通过一个option来启用)。

```cpp
    user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, nullptr, &regs);
```

其中`user_regs_struct`定义了所有的x86下的寄存器，可以通过`/usr/include/sys/user.h`这个头文件来查看这个结构的定义。通过`ptrace`的`PTRACE_GETREGS`来获取所有寄存器的值。而修改
寄存器的值则可以通过`PTRACE_SETREGS`来完成。内存查看和修改是通过上文中提到的`PTRACE_PEEKDATA`和`PTRACE_POKEDATA`来完成。接下来要讲一下调用堆栈的获取。调用堆栈依赖于x86的栈帧的布局。



## 调试信息




## 总结