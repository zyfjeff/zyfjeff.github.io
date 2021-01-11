# 负载均衡算法

[Load balancing](https://en.wikipedia.org/wiki/Load_balancing_(computing))根据Wiki的定义，负载均衡指的是在一组资源（计算单元）上分配一组任务的过程，目的是使它们的整体处理效率更高。
负载均衡算法可以优化每个任务的响应时间，避免在其他计算节点处于空闲状态时不均匀地使计算节点过载。常见的负载均衡算法有随机、轮询、最少连接等，这些算法还可以结合权重。在结合权重的情况下如何能高效的
选取一个节点，并且保证负载均衡是衡量算法的重要特征，本文主要关注一些开源项目中使用的基于权重的一些高效负载均衡算法的分析。

## Smooth weighted round-robin

这是Nginx使用的基于权重的轮询算法，名为Smooth，顾名思义该算法相比于其它加权轮询（WRR）算法多一个 smooth（平滑）的特性。
这个算法的核心思想是，每次选择一台权重最大的机器后，用这个机器的权重减去总的权重，下次选择的时候

```plain
Upstream: smooth weighted round-robin balancing.

For edge case weights like { 5, 1, 1 } we now produce { a, a, b, a, c, a, a }
sequence instead of { c, b, a, a, a, a, a } produced previously.

Algorithm is as follows: on each peer selection we increase current_weight
of each eligible peer by its weight, select peer with greatest current_weight
and reduce its current_weight by total number of weight points distributed
among peers.

In case of { 5, 1, 1 } weights this gives the following sequence of
current_weight's:

     a  b  c
     0  0  0  (initial state)

     5  1  1  (a selected)
    -2  1  1

     3  2  2  (a selected)
    -4  2  2

     1  3  3  (b selected)
     1 -4  3

     6 -3  4  (a selected)
    -1 -3  4

     4 -2  5  (c selected)
     4 -2 -2

     9 -1 -1  (a selected)
     2 -1 -1

     7  0  0  (a selected)
     0  0  0
```

2. LVS

```bash
Supposing that there is a server set S = {S0, S1, …, Sn-1};
W(Si) indicates the weight of Si;
i indicates the server selected last time, and i is initialized with -1;
cw is the current weight in scheduling, and cw is initialized with zero;
max(S) is the maximum weight of all the servers in S;
gcd(S) is the greatest common divisor of all server weights in S;

while (true) {
    i = (i + 1) mod n;
    if (i == 0) {
        cw = cw - gcd(S);
        if (cw <= 0) {
            cw = max(S);
            if (cw == 0)
            return NULL;
        }
    }
    if (W(Si) >= cw)
        return Si;
}
```

3. Envoy EDF

通过这个公式来计算每一个机器的`deadline`时间，`current_time_`初始为0
`const double deadline = current_time_ + 1.0 / weight;`然后会按照deadline的大小
建立小根堆，deadline最小的在堆顶，优先出队，出队后current_time_就等于新出队的元素的deadline时间
可以理解成惩罚因子，通过这个值可以将下一个入队的主机的deadline时间变大。而不至于每次把高权重的机器加入
就会导致这个机器被再次选中。


## HashTable/HashMap

hash map或者说是hash table通常来说是用于实现高效率查找的一个数据结构，他的核心在于hash算法和数据结构，以及如何处理hash冲突等。典型的实现方式如下:

拥有一个数组，这个数组的大小要尽量大于我们实际存放的元素数量，然后选取一个hash函数对key进行hash，并除以这个数组的大小得到一个index即可。
这个带来的问题就是数组中存在大量没有使用的条目，会带来内存上的开销(需要解决)。如果这个数组太小又会导致冲突概率比较高，如果发生冲突是需要解决的，典型的
解决方式有链表法、二次hash等等

1. SwissTable/hashbrown
2. Sparsehash
3. unordered_map
4. folly f4

Reference:

[weighted](https://github.com/smallnest/weighted)
[Sparsehash](http://tristanpenman.com/blog/posts/2017/10/11/sparsehash-internals/)
[SeaHash](http://ticki.github.io/blog/seahash-explained/)
[SwissTable](https://abseil.io/blog/20180927-swisstables)
[hashbrown](https://blog.waffles.space/2018/12/07/deep-dive-into-hashbrown/)
[Benchmark of major hash maps implementations](https://tessil.github.io/2016/08/29/benchmark-hopscotch-map.html)
[VNSWRR](https://www.infoq.cn/article/sebuh0k6ji*ytfqzcihb)