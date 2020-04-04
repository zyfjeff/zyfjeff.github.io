
## 关闭地址空间随机化(ALSR)

0 - 表示关闭进程地址空间随机化。
1 - 表示将mmap的基址，stack和vdso页面随机化。
2 - 表示在1的基础上增加栈（heap）的随机化。

```shell
# echo 0 >/proc/sys/kernel/randomize_va_space
```
