## Ingress - they're coming

1. Packets arrive at the NIC
2. NIC will verify MAC (if not on promiscuous mode) and FCS(FCS：Frame Check Sequence（帧校验序列) and decide to drop or to continue
3. NIC will [DMA packets at RAM](https://en.wikipedia.org/wiki/Direct_memory_access), in a region previously prepared (mapped) by the driver

Packet is copied (via DMA) to a ring buffer in kernel memory.

```
// 网卡设备的ring buffer大小
Check command: ethtool -g ethX
Change command: ethtool -G ethX rx value tx value
```

4. NIC will enqueue references to the packets at receive ring buffer queue rx until rx-usecs(超时时间，只有到了这个超时时间才触发硬中断，避免大量中断) timeout or rx-frames(网卡收到指定数量的包，才会触发硬件中断)

```
# ethtool –C eth1 // 查看网卡的当前设置
# ethtool –C eth1 rx-usecs 450 // 设置网卡的rx-usecs参数
# Valid range for rx-usecs are: 0,1,3, 10-8191 // 网卡的取值范围

1. 0= off,  real-time interruption,  one package one interrupt, the lowest delay
2. 1=dynamic，which rangefor interrupts is 4000-70000
3. 3=dynamicconservative(default)，which range for interrupts is 4000-20000
4. 10-8191，how many microseconds every interruption will occurs. For example,you may wish to control the number of interrupt  in every second less than 1000 , then setrx-usecs to 1000000/1000=1000 microseconds, if less than 2000, then set it to1000000/2000=500
```

5. NIC will raise a hard IRQ
6. CPU will run the IRQ handler that runs the driver's code
7. Driver will schedule a NAPI, clear the hard IRQ and return

```
NAPI：数据包到来，第一个数据包产生硬件中断，中断处理程序将设备的napi_struct结构挂在当前cpu的待收包设备链表softnet_data->poll_list中，并触发软中断，软中断执行过程中，遍历softnet_data->poll_list中的所有设备，依次调用其收包函数napi_sturct->poll，处理收包过程；

非NAPI：每个数据包到来，都会产生硬件中断，中断处理程序将收到的包放入当前cpu的收包队列softnet_data->input_pkt_queue中，并且将非napi设备对应的虚拟设备napi结构softnet->backlog结构挂在当前cpu的待收包设备链表softnet_data->poll_list中，并触发软中断，软中断处理过程中，会调用backlog的回调处理函数process_backlog，将收包队列input_pkt_queue合并到softdata->process_queue后面，并依次处理该队列中的数据包；
```

8. Driver raise a soft IRQ (NET_RX_SOFTIRQ)
9. NAPI will poll data from the receive ring buffer until netdev_budget_usecs(poll的超时时间) timeout or netdev_budget(最低需要处理的报文数量) and dev_weight(一次poll累计处理的最大报文数) packets

```
net.core.netdev_budget 可以poll的报文数量，低于这个数值就停止poll，
net.core.netdev_budget 可以poll的时间
net.core.dev_weight poll的报文数量的最大值
```

10. Linux will also allocated memory to sk_buff
11. Linux fills the metadata: protocol, interface, setmacheader, removes ethernet
12. Linux will pass the skb to the kernel stack (netif_receive_skb)
13. It will set the network header, clone skb to taps (i.e. tcpdump) and pass it to tc ingress


Reference: http://tldp.org/HOWTO/Traffic-Control-HOWTO/intro.html

14. Packets are handled to a qdisc sized netdev_max_backlog with its algorithm defined by default_qdisc

```
net.core.somaxconn    已经连接的socket，但是还没有被accept的队列大小
/proc/sys/net/ipv4/tcp_max_syn_backlog  半连接队列的大小(已经返回了sync+ack报文了，等待ack)
/proc/sys/net/core/netdev_max_backlog 网卡设备的队列大小，只有在开启PRS才有作用
```

15. It calls ip_rcv and packets are handled to IP
16. It calls netfilter (PREROUTING)
17. It looks at the routing table, if forwarding or local
18. If it's local it calls netfilter (LOCAL_IN)

<img src="../images/ip_process.jpg" width = "1000" height = "800" alt="archite" align="center" />

19. It calls the L4 protocol (for instance tcp_v4_rcv)
20. It finds the right socket
21. It goes to the tcp finite state machine
22. Enqueue the packet to the receive buffer and sized as tcp_rmem rules

```
Check command: sysctl net.ipv4.tcp_rmem
Change command: sysctl -w net.ipv4.tcp_rmem="min default max"; when changing default value,
remember to restart your user space app (i.e. your web server, nginx, etc)
```

23. If tcp_moderate_rcvbuf is enabled kernel will auto-tune the receive buffer

```
net.ipv4.tcp_moderate_rcvbuf = 1 // 自动调整接收buffer的大小
```

24. Kernel will signalize that there is data available to apps (epoll or any polling system)
25. Application wakes up and reads the data


Reference: https://github.com/leandromoreira/linux-network-performance-parameters#linux-network-queues-overview



## Egress - they're leaving
1. Application sends message (sendmsg or other)
2. TCP send message allocates skb_buff
3. It enqueues skb to the socket write buffer of tcp_wmem size

```
Check command: sysctl net.ipv4.tcp_wmem
Change command: sysctl -w net.ipv4.tcp_wmem="min default max"; when changing default value,
remember to restart your user space app (i.e. your web server, nginx, etc)
```

4. Builds the TCP header (src and dst port, checksum)
5. Calls L3 handler (in this case ipv4 on tcp_write_xmit and tcp_transmit_skb)
6. L3 (ip_queue_xmit) does its work: build ip header and call netfilter (LOCAL_OUT)
7. Calls output route action
8. Calls netfilter (POST_ROUTING)
9. Fragment the packet (ip_output)
10. Calls L2 send function (dev_queue_xmit)
11. Feeds the output (QDisc) queue of txqueuelen length with its algorithm default_qdisc

```
// 调整txqueuelen输出队列的大小
Check command: ifconfig ethX
Change command: ifconfig ethX txqueuelen value

// 调整qdisc的策略
Check command: sysctl net.core.default_qdisc
Change command: sysctl -w net.core.default_qdisc value
```

12. The driver code enqueue the packets at the ring buffer tx
13. The driver will do a soft IRQ (NET_TX_SOFTIRQ) after tx-usecs timeout or tx-frames
14. Re-enable hard IRQ to NIC
15. Driver will map all the packets (to be sent) to some DMA'ed region
16. NIC fetches the packets (via DMA) from RAM to transmit
17. After the transmission NIC will raise a hard IRQ to signal its completion
18. The driver will handle this IRQ (turn it off)
19. And schedule (soft IRQ) the NAPI poll system
20. NAPI will handle the receive packets signaling and free the RAM



## How to monitor

* `cat /proc/interrupts` 看中断数量
* `cat /proc/net/softnet_stat` 网络软中断情况，可以通过`code/softnet.sh`来输出

```
每一行对应到一个CPU:

第一例: 处理的包数量，在boding模式下，相同packet会被记录多次
第二例: dropped的包数量
第三例: poll因为budget或者时间的问题被中断的次数
第四~八例: 总是0
第九例: CPU冲突的数量
第十例: CPU因为inter-processor Interrupt唤醒的次数
最后一例: 被流控限制的数量 (流控限制是RPS(Receive Packet Steering)的一个feature)
```

* `cat /proc/net/sockstat`

```
sockets: used 159
TCP: inuse 49 orphan 0 tw 2 alloc 61 mem 487
UDP: inuse 0 mem 0
UDPLITE: inuse 0
RAW: inuse 0
FRAG: inuse 0 memory 0

mem代表了消耗的内存，单位是PAGE SIZE，一般为4KB。487 * 4KB即为，内核处缓存用户没有及时收取到用户空间的tcp数据所消耗的内存大小。
```
* `netstat -atn | awk '/tcp/ {print $6}' | sort | uniq -c` or `ss -s`
* `ss -neopt state time-wait | wc -l`
counters by a specific state: established, syn-sent, syn-recv, fin-wait-1, fin-wait-2, time-wait, closed, close-wait, last-ack, listening, closing
* `netstat -st` or `nstat -a`
* `cat /proc/net/tcp`
detailed stats, see each field meaning at the [kernel docs](https://www.kernel.org/doc/Documentation/networking/proc_net_tcp.txt)
* `cat /proc/net/netstat`
ListenOverflows and ListenDrops are important fields to keep an eye on
* `/sys/class/net/eth0/statistics/` or `ethtool -S eth0` 监控网卡的统计信息
* `/proc/net/dev` high level级别的网卡统计信息

## Tuning

* RSS(Receive Side Scaling) / Receive Packet Steering (RPS)/ multiqueue

* 调整网卡队列的数量

```
sudo ethtool -l eth0 //  查看队列数量
sudo ethtool -L eth0 combined 8 // 调整接收队列和发送队列数量为8
sudo ethtool -L eth0 rx 8 // 独立调整接收队列数量为8
```

* 调整网卡队列的大小

```
sudo ethtool -g eth0  // 查看队列大小
sudo ethtool -G eth0 rx 4096 // 设置接收队列的大小
```

* 调整队列的权重

```
sudo ethtool -x eth0 // 查看网卡的hash值到queue的映射关系
sudo ethtool -X eth0 equal 2 // eth0网卡使用前2个queue均匀来处理数据包
sudo ethtool -X eth0 weight 6 2 // 第一个queue的权重是6、第二个是2
```

* 调整队列的hash数据源

```
sudo ethtool -n eth0 rx-flow-hash udp4 // 查看udp的rx队列中的hash算法的数据源
sudo ethtool -N eth0 rx-flow-hash udp4 sdfn // 设置udp的rx队列的hash算法的数据源为sdfn
```

* ntuple filtering(用于设置根据某些条件，将数据包hash到指定的queue上)

```
sudo ethtool -k eth0  // 查看当前网卡的ntuple filters
sudo ethtool -K eth0 ntuple on // Enable ntuple filters
sudo ethtool -U eth0 flow-type tcp4 dst-port 80 action 2 // tcp流，带有80端口，hash到接收队列2上
```

* Interrupt coalescing(中断合并，只有达到一定数量的数据包达到才会触发中断)

```
sudo ethtool -c eth0
sudo ethtool -C eth0 adaptive-rx on
1. rx-usecs
2. rx-frames
3. rx-usecs-irq
4. rx-frames-irq
```

* Adjusting IRQ affinities

1. 需要关闭`irqbalance`
2. 查看`/proc/interrupts`需要绑定CPU的网卡中断号
3. `sudo bash -c 'echo 1 > /proc/irq/8/smp_affinity'` // 绑定irq number为8的queue到CPU0上

* 调整NAPI poll的相关参数

```
net.core.netdev_budget 可以poll的报文数量，低于这个数值就停止poll，
net.core.netdev_budget_timeout 可以poll的时间
net.core.dev_weight poll的报文数量的最大值
```

* LRO(Large Receive Offloading)/GRO(Generic Receive Offloading)

LRO是硬件实现、GRO是软件实现，其目的是为了将packet合并放到network stack中进行处理，以此减少CPU的处理时间，但是LRO合并packet过于宽泛
，会存在信息丢失的问题，因为某些packet会携带一些标志，合并packet后会丢失这些标志。而GRO处理更为严格。

```
sudo ethtool -k eth0 | grep generic-receive-offload // 查看是否开启GRO
sudo ethtool -K eth0 gro on // 开启GRO
```

* Receive Packet Steering (RPS)/Receive Side Scaling (RSS)

```
echo "10000000" > /sys/class/net/eth0/queues/rx-0/rps_cpus  // eth0网卡的接收队列0，映射到CPU0
```

* Receive Flow Steering (RFS)

PRS/RSS 只能将包平均的分发给各个CPU，但是没办法最大化的利用CPU Cache，相同流的数据包不一定会分配到同一个CPU中，而RFS则是解决这个问题的
会将相同流的数据包让相同的CPU来处理，提供CPU的缓存命中率。

```
sudo sysctl -w net.core.rps_sock_flow_entries=32768  // 设置RFS socket hash表大小
sudo bash -c 'echo 2048 > /sys/class/net/eth0/queues/rx-0/rps_flow_cnt' //每个queue最大的流数量
```

* Enabling accelerated RFS (aRFS)

1. RPS开启
2. RFS开启
3. 内核编译的时候，配置了CONFIG_RFS_ACCEL参数
4. 设备支持ntuple
5. RX queue和CPU一对一映射配置好了

只要上述条件满足就自动开启aRFS

* RX packet timestamping (在进入RPS之前打上时间戳，还是之后，默认是之前)

```
// 在进入RPS之后，才开始打上时间戳，而不是之前
sudo sysctl -w net.core.netdev_tstamp_prequeue=0
```

* Enabling flow limits and tuning flow limit hash table size

```
sudo sysctl -w net.core.flow_limit_table_len=8192
/proc/sys/net/core/flow_limit_cpu_bitmap
```

*  adjusting IP protocol early demux

在进去路由表进行路由匹配的时候先看下是否有路由缓存，默认是开启的，在某些场景下开启会导致5%的性能下降

```
sudo sysctl -w net.ipv4.ip_early_demux=0
```

* `SO_INCOMING_CPU` 结合getsockopt，可以用于查看当前socket在被哪个CPU处理

Reference: https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/#special-thanks
Reference: https://vincent.bernat.ch/en/blog/2014-tcp-time-wait-state-linux




## TCP connection repair

https://lwn.net/Articles/495304/