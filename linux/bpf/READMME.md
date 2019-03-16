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


## bcc

```python
// 1. 第一种形式
BPF(text="BPF程序代码").trace_print()

// 2. 第二种形式
b = BPF(text="BPF程序代码")
b.attach_kprobe(event="syscall名称", fn_name = "回调BPF程序代码中对应名称的函数")
b.trace_fields() //将输出信息按照字段分割的形式输出  (task, pid, cpu, flags, ts, msg)

```

1. `bpf_ktime_get_ns` 获取当前时间，单位是nanoseconds
2. `BPF_HASH(last)` 创建名为last的关联数组，如果没有指定额外参数的化，key和value的类型都是u64
3. `last.lookup(&key)` 查询key是否在hash中，不在就返回NULL
4. `last.delete(&key)` 从hash中删除key
5. `last.update(&key, &value)` 更新数据
6. `bpf_trace_printk` bpf程序输出
7. bpf程序中所有要运行的函数，其第一个参数都需要是`struct pt_regs*`
8. `bpf_get_current_pid_tgid` 返回进程PID
9. `bpf_get_current_comm` 获取当前程序名称
10. `BPF_PERF_OUTPUT(events)` 定义输出的channel名称
11. `events.perf_submit()` 提交event到用户空间
12. `b["events"].open_perf_buffer(print_event)` 把输出函数和输出的channel关联起来
13. `b.perf_buffer_poll()` 阻塞等待events
14. `BPF_HISTOGRAM`定义BPF Map对象，这是一个histogram(dist.increment()bucket递增)
15. `bpf_log2l` 返回log2函数计算的结果。
16. `b["dist"].print_log2_hist("kbytes")` 按照log2作为key，kbytes作为header，打印dist这个histogram中的数据
17. `attach_kretprobe` attach到一个内核函数的`return`点
18. `BPF(src_file = "vfsreadlat.c")` 从源码中读取BPF程序
19. `attach_uprobe` attach到一个uprobe
20. `PT_REGS_PARM1` 获取到要trace的函数中的第一个参数
21. `bpf_usdt_readarg(6, ctx, &addr)` 读取USDT probe的第六个参数到addr变量中
22. `bpf_probe_read(&path, sizeof(path), (void *)addr)` 将addr指向path变量
23. `USDT(pid=int(pid))` 对指定PID开启USDT tracing功能
24. `enable_probe(probe="http__server__request", fn_name="do_trace")` attach do_trace函数到Node.js的`http__server__request` USDT probe
25. `BPF(text=bpf_text, usdt_contexts=[u])` 将USDT对象u传递给BPF对象


## uprobe

核心就是`b.attach_uretprobe(name=name, sym="readline", fn_name="printret")` 这段代码，共享库位置/二进制位置，要uprobe的符号名称，触发的function

```python
# load BPF program
bpf_text = """
#include <uapi/linux/ptrace.h>
struct str_t {
    u64 pid;
    char str[80];
};
//Setp1: 定义输出的events
BPF_PERF_OUTPUT(events);
int printret(struct pt_regs *ctx) {
    // Setp2: 自定义event的结构
    struct str_t data  = {};
    u32 pid;
    if (!PT_REGS_RC(ctx))
        return 0;
    pid = bpf_get_current_pid_tgid();
    data.pid = pid;
    bpf_probe_read(&data.str, sizeof(data.str), (void *)PT_REGS_RC(ctx));
    // Setp3: 提交event
    events.perf_submit(ctx,&data,sizeof(data));
    return 0;
};
"""
b = BPF(text=bpf_text)
# name是共享库或者是可执行程序到达名称
b.attach_uretprobe(name=name, sym="readline", fn_name="printret")
# header
print("%-9s %-6s %s" % ("TIME", "PID", "COMMAND"))

def print_event(cpu, data, size):
    event = b["events"].event(data)
    print("%-9s %-6d %s" % (strftime("%H:%M:%S"), event.pid,
                            event.str.decode('utf-8', 'replace')))

b["events"].open_perf_buffer(print_event)
while 1:
    try:
        b.perf_buffer_poll()
    except KeyboardInterrupt:
        exit()
```