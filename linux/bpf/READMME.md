## XDP generic 和 XDP native
前者是在网卡收到包后，创建skb的时候(需要设置XDP_FLAGS_SKB_MODE启用)，作用在Linux内核层面，
后者则是直接在网卡驱动层。前者拿到包后可以任意处理，但是后者要redirect的时候，只能redirect到另外一个支持XDP native的设备中。

## eBPF map
per cpu的，多个实例共享同一个map。避免了多个CPU cache之间进行同步

## tail call
多个eBPF程序可以级连，通过`tail call`来调用另外一个eBPF程序。

## eBPF工作流

1. Create an eBPF program
2. Call new new bpf() syscall to inject the program in the kernel and obtain a reference to it
3. Attach the program to a socket (with the new SO_ATTACH_BPF setsockopt() option):
  * setsockopt(socket, SOL_SOCKET, SO_ATTACH_BPF, &fd, sizeof(fd));
  * where “socket” represents the socket of interest, and “fd” holds the file descriptor for the loaded eBPF program
4. Once the program is loaded, it will be fired on each packet that shows up on the given socket
5. limitation: programs cannot do anything to influence the delivery or contents of the packet
  * These programs are not actually “filters”; all they can do is store information in eBPF “maps” for consumption by user space

## USDT(Userland Statically Defined Tracepoints)
DTRACE_PROBE，定义用户态的tracepoint，需要引入systemtap-sdt-dev包，并包含`#include <sys/sdt.h>`头文件

eBPF载入程序后会进行深度搜索CFG来检测，如果发现不可达的指令就禁止执行、

## BPF verifier.
1. Providing a verdict for kernel whether safe to run
2. Simulation of execution of all paths of the program
3. Steps involved (extract):
  1. Checking control flow graph for loops
  2. Detecting out of range jumps, unreachable instructions
  3. Tracking context access, initialized memory, stack spill/fills
  4. Checking unpriviledged pointer leaks
  5. Verifying helper function call arguments
  6. Value and alignment tracking for data access (pkt pointer, map access)
  7. Register liveness analysis for pruning
  8. State pruning for reducing verification complexity