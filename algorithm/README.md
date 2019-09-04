## Manacher算法

1. 任何字符串，无论奇数和偶数，只要给每一个字符左右两边添加一个特殊字符，就会使得这个字符串变成奇数个，这个在回文的场景下特别有用。


## 负载均衡算法

1. Nginx: smooth weighted round-dobin

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


Reference:

[weighted](https://github.com/smallnest/weighted)