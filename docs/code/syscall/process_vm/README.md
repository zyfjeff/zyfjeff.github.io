## Guide
process_vm_readv/writev 可以用于两个进程之间直接内存读写通信，而且这两个进程并不需要有任何关系(比如父子进程、同一个group的、或者是相同会话的)
process_vm.c例子中通过父子进程使用socketpair进行父子进程通信，获取到对方的进程pid，然后子进程通过socketpair将要被读取的内存地址告诉父进程
父进程通过process_vm_readv来读取指定内存位置的内容，然后输出。

## Tips
1. 任何进程之间都可以通过process_vm_readv/writev进行相互通信(要有权限)
2. 要读取和写入的地址需要是进程级别的虚拟地址，因此需要对方告知自己
