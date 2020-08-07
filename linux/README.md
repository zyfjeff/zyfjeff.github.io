
## 关闭地址空间随机化(ALSR)

0 - 表示关闭进程地址空间随机化。
1 - 表示将mmap的基址，stack和vdso页面随机化。
2 - 表示在1的基础上增加栈（heap）的随机化。

```shell
# echo 0 >/proc/sys/kernel/randomize_va_space
```



* `pthread_atfork` 可以注册fork前、parent、child等回调做一些清理的动作

* `_exit` 不会析构全局对象、也不会执行任何清理动作

* signal handler中只能调用异步信号安全的函数，并且如果需要修改全局数据的时候，那么被修改的变量必须是sig_atomic_t类型的，否则被打断的函数在恢复执行后很可能不能立刻看到signal
  handler改动后的数据。因为编译器有可能假定这个变量不会被他处修改，而从优化了内存访问。
  