## 工具篇

* pidstat 进程级别的性能分析

  1. -u CPU使用率分析
    * `%guest` 花费在运行虚拟机的cpu 百分比
    * `%wait` 花费在等待CPU去运行的CPU百分比，这个值越高说明CPU不足比较严重

> stress-ng --cpu 10 --timeout 600 通过stress-ng模拟超过CPU核心数的进程运行时就会出现%wait值很高的场景

  2. -d IO分析
    * `kB_rd/s`   每秒磁盘读取的kb数，注意这里是读次盘，并不是读缓存
    * `kB_wr/s`   每秒磁盘写入的kb数
    * `kB_ccwr/s` 每秒因为truncate导致page cache dirty被撤销的kb数
    * `iodelay` 等待`同步io完成`和因为`swap in而导致的block IO完成`所需要等待的时钟周期

  3. -w 上下文切换分析
    * `cswch/s` 主动让出CPU导致的上下文切换(比如等待IO)
    * `nvcswch/s` 被动让出CPU导致的上下文切换(比如时间片用完)

> IO密集型的进程，cswch/s的值会更高一些
> 默认pidstat显示的数进程级别的线程切换次数，但是上下文切换其实是线程级别的，所以默认只会显示主线程的上下文切换次数，需要加上-t显示所有线程的切换次数
> 上下文切换次数增多会导致中断次数也增多，中断有很多种，其中一种叫做RES(重调度中断，通过/proc/interrupts查看)中断，让空闲的CPU来调度任务。

* mpstat 系统级别的CPU分析，分析多核CPU的整体情况，以及各个CPU的使用率、iowait等
mpstat -P ALL interval

* dstat

* top 基于proc文件快照实现，对于生命周期比较短的进程就无能为力了

  * 按下M切换到内存排序
  * 按下P切换到CPU使用率排序

```bash
As a default, percentages for these individual categories are displayed.
Where two labels are shown below, those for more recent kernel versions are shown first.

  us, user    : time running un-niced user processes
  sy, system  : time running kernel processes
  ni, nice    : time running niced user processes
  id, idle    : time spent in the kernel idle handler
  wa, IO-wait : time waiting for I/O completion
  hi : time spent servicing hardware interrupts
  si : time spent servicing software interrupts
  st : time stolen from this vm by the hypervisor
```

* iostat 查看磁盘的读、写速度、以及cpu的iowait、使用率等


* vmstat 分析系统级别的上下文切换、进程队列大小、中断数等

> top、pidstat、ps等查看CPU使用率的时候，会存在不一致的问题，本质上是因为，这些工具计算CPU使用率的时候用的都是平均CPU使用率
> 也就是计算一段时间内的平均CPU使用率，top默认是3秒计算一次。而ps看的是整个进程的生命周期。

* sar

  * -r 显示内存使用情况
  * -S 显示Swap的使用情况


  * `iowait` 表示在一个采样周期内有百分之几的时间属于以下情况：CPU空闲、并且有仍未完成的I/O请求。iowait高并不代表I/O有瓶颈
  因为iowait的值取决于CPU空闲状态下的未完成I/O请求，意思就是I/O未完成不是因为没有CPU导致的，而是在等待IO。所以这个值会受到CPU是否
  空闲的影响，如果CPU很繁忙，那么这个值不受影响。一旦CPU空闲了，这个值就升高了，但是前后的IO没有变化。


echo 3 > /proc/sys/vm/drop_caches

* pcstat 可以用来查询一个文件是否被缓存了，核心是使用了mincore系统调用

* bcc的cachestat.py、cachetop.py、memleak.py、


* strace -p xxx -c 统计系统调用的次数和耗时

## 场景case

1. 大量进程频繁创建和退出导致的CPU使用率上升，但是ps、top、pidstat无法查看到这一现象

top、ps、pidstat这些都是基于一段间隔来统计和计算，没办法对于那些生命周期非常短的大量进程导致的CPU使用率变高的分析，需要结合perf record来分析。

2. 直接IO读磁盘，导致iowait升高，但是CPU使用率不高，load也升高了。


## 基本原理

* Linux调度器原理和优先级

调度策略有SCHED_FIFO、SCHED_RR、SCHED_BATCH、SCHED_IDLE、SCHED_NORMAL，前两个是实时进程的调度策略，默认是SCHED_NORMAL，可以通过sched_setscheduler来调整调度策略和优先级。

  * 非实时进程，通过nice(进程优先级，范围-20~19)来调整，在进程内部可以调用getpriority/setpriority来设置，这个nice只是权重，并不是绝对的。
  * 实时进程其优先级范围为1~99

实时进程一旦运行会一直运行，除非被高优先级的进程抢占、进程终止、阻塞(等待IO)、自动调用sched_yield放弃CPU。

* 进程上下文切换的场景

  * 时间片用完了
  * 进程资源不足，等待IO或者等待内存资源
  * 进程主动sleep
  * 进程主动让出cpu(实时进程才能这么做)
  * 优先级抢占
  * 硬件中断

* 内存不足时如何处理
  * OOM来杀死进程 (oom_score来确定哪个进程被杀死，消耗的内存越大oom_score越大，越大越容易杀死，[-17, 15]，可以/proc/$PID/oom_adj 来挑战这个值)
  * swap处理，将不常使用的匿名页换出
  * 把最近使用较少的缓存页回收

* Buffer和Cache

Buffer是对磁盘数据的缓存，而Cache是文件数据的缓存，它们既会用在读请求中，也会 用在写请求中。
理论上，一个文件读首先到Block Buffer, 然后到Page Cache。有了文件系统才有了Page Cache.
在老的Linux上这两个Cache是分开的。那这样对于文件数据，会被Cache两次。这种方案虽然简单， 但低效。
后期Linux把这两个Cache统一了。对于文件，Page Cache指向Block Buffer，对于非文件 则是Block Buffer。
这样就如文件实验的结果，文件操作，只影响Page Cache，Raw操作，则只影响Buffer.



  * 内存分类

从几个纬度分为下面四个象限。

```plain
                            Private | Shared
                        1           |          2
  Anonymous   . stack               |
              . malloc()            |
              . brk()/sbrk()        | . POSIX shm*
              . mmap(PRIVATE, ANON) | . mmap(SHARED, ANON)
            -----------------------+----------------------
              . mmap(PRIVATE, fd)   | . mmap(SHARED, fd)
File-backed   . pgms/shared libs    |
                        3           |          4
```

匿名页、基于文件映射、privated、shared的，内核在


## 性能分析

* CPU
  1. `CPI/IPC` 每指令周期数/每周期指令数，这个数值越高，表明CPU经常陷入停滞，通常是在访问内存。(perf stat)
  2. `使用率`(mpstat、top)
  3. `用户时间/内核时间` 计算密集型用户时间多，IO密集型则内核时间多。(mpstat、top)
  4. `缓存亲和性` 进程CPU绑定、cpuset独占CPU组等方式，避免进程跨CPU切换导致缓存失效。(perf stat)
  5. `调度策略`
  6. `上下文切换开销` (vmstat、pidstat -w)


通过cpuset实现独占CPU组

```bash
cd /sys/fs/cgroup/cpuset
mkdir prodset
cd prodset
echo 7-10 > cpus
echo 1 > cpu_exclusive
echo PID > tasks
```

通过taskset进行进程绑定

```bash
taskset -pc 7-10 PID
```

* Memory
  1. `TLB缓存命中率` HugePage来提高TLB缓存命中率，还有THP(透明巨页面)
  2. `swappiness` swap交换的倾向性
  3. `oom`  /proc/PID/oom_adj [-17, 15]数值越大，越容易被OOM杀死，cgroup中通过memory.oom 来开启或者禁用oom
  4. `脏页回收策略`
  5. `页缓存和Buffer`
  6. `缺页异常`
  7. `slab缓存` (slabtop)

1. /proc/sys/vm/min_free_kbytes 定期回收内存的阀值
2. /proc/sys/vm/swappiness 文件页和匿名页的回收倾向


## Linux proc

* /proc/PID/smaps

  1. PSS 把共享内存平分到各个进程后，再加上进程本身非共享内存的大小

The "proportional set size" (PSS) of a process is the count of pages it has
in memory, where each page is divided by the number of processes sharing it.
So if a process has 1000 pages all to itself, and 1000 shared with one other
process, its PSS will be 1500.


## Linux perf

perf record -e 事件/probe options

-a trace all cpu
-g capture call graphs

* Counting Events:

perf stat:

-d Detailed CPU counter statistics
-p specified PID
-a CPU counter statistics for the entire system


## Tcmalloc perf

yum install pprof graphviz


* heapprofiler
pprof --base=/var/log/envoy/envoy.prof.0001.heap --svg /home/admin/alimesh/bin/envoy /var/log/envoy/envoy.prof.0008.heap


* cpuprofiler

## Link
[理解 %IOWAIT (%WIO)](http://linuxperf.com/?p=33)