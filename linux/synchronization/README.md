## Linux lock types

* Blocking: mutex, semaphore
* Non-blocking: spinlocks, seqlocks, completions

## sync lock基础

* hardware有那些intrinsic可以帮助实现lock
* 其中各自的特点（在不同场景优点，缺点）是什么，请做性能分析预测
* 可以对lock做出哪些优化，以及为什么做这些优化

60年代的时候，Dijkstra和Knuth都证明了，在符合sequential consistency的memory system里，只需atomic read和write就可以实现mutual exclusion

常见的intrinsic有:

IBM：compare&swap。Intel推出的是LOCK prefix。MIPS的更灵活一些：LL/SC

一个sync操作，分为三个部分: 1. Accquire、2. Waiting、3. Release

```c
lock   // Acquire and Waiting
critical_section;
unlock // Release
```

在Acquire部分，所有thread必须竞争，选出一个获胜者，losers则进入waiting(spin lock是busy waitting、mutex则是sleep)。
Winner完成critical_section后，Winner释放lock，而那些正在busy waiting的losers再一次竞争，选出下一个Winner。

Acquire需要使用原子的`read-modify-write`操作，典型的实现方式为Test And Set。

```c
Test_And_Set(int *p) {
  old = *p;
  *p = 1;   // 假设p的初始值为0，每次我们都试图写入1
  return old
}

lock(int *p) {
  while(Test_And_Set(p) == 1);  // 直到返回0为止
}
```

这种`Test And Set`的方式存在一个问题就是每次无论是否Accquire都需要进行一次写操作，这会带来一个问题就是，一个winner在写入之后，成为p所在cache line的owner；
但立刻就有一个loser issue了一个write，逼winner放弃了owner的权利；立刻又有一个loser干了同样的事。所有的竞争者都在不停的制造大量的bus transaction，
而p所在的cache line反复的被切换owner。

Test_And_Set本身是有改进空间的。因为我们知道了bus transaction以及cacheline owner flip的问题之后，相应的解决办法就是减少bus transaction。

1. `backoff` 当loser发现自己失败后，稍微等一下再接着竞争，但是到底等待多长时间是一个问题。
2. `Test And Test and Set` 在进入Test And Set之前先判断下lock是否是0，只有不为0的时候才进入Test_And_Set。这样可以显著减少writer的次数，并且有相当数量的reader可以命中
local cache(读lock)。

```c
while(0 != atomic_dec(&lock->counter)) {
  do {
    // Pause the CPU until some coherence
    // traffic(a prerequisite for the counter changing)
    // saving power
  } while(lock->counter <= 0);
}
```

`Test And Test and Set`的这种方式仍然存在问题，就是当winner release lock的时候所有的loser都会被同时唤醒，于是他们会发起竞争，造成burst traffic。
另外一个问题则是公平性的问题，所有的线程在第一次竞争都是公平的，但是一旦有winner之后，winner就会一直占据优先权，这是因为winner能比其它的线程更早看到P的值，winner
在Release的时候会进行写入操作，这个操作会先进入store buffer，然后再进入ram，而从其它loser角度来看，他们会先体验一次read miss，然后再从ram里读到p的值。这个时间差
导致了winner能更早读到p的值。


## Linux kernel TicketLock

其基本概念就是大家去银行排队的时候，去取票机拿一张票，上面写着一个号。然后大家不停的看当前服务的号是几。
如果跟自己的手中的票号一样，那么进去接受服务。等自己出来的时候，别忘了把当前服务号+1，让下一位顾客有机会进来。

这个算法解决了公平性的问题，winner失去了优先权，而且只有再winner Release的时候才会产生一个write/invalidate。但是这个算法还是有问题，当winner改变服务号的时候，所有的
loser都会被invalidate。有没有可能再unlock的时候只唤醒一位loser呢? 也就是只让一个loser产生read miss，而其他的loser完全察觉不到cacheline的改变呢?


* Array-based lock

整个系统里有一个array，其slot的数量等于thread的数量，有一个全局的index，初始化为0，当开始竞争的时候，每个thread试图增加index，抢到一个index后就开始spin。

```c
lock:
      int my_index = fetch_n_add(index);
      while (array[my_index] == 0);

unlock:
     array[my_index+1] == 1;
```

这个算法的优点就是，每一个loser只关心自己所在的slot，而不是全都关心一个全局地址的值。，而winner也只解锁排在自己后面的loser。这样的话，当winner unlock的时候，只有
一个loserhi发现自己的cache line被invalidate。这个算法的缺点就是空间复杂度由O(1)变成了O(n)。

这个算法还存在另外一个问题就是跨numa节点访问内存的问题，如果thread分布在不同socket上，那么因为他们并不知道自己会得到array中哪个index，所以必然
会有一部分thread得spin在远程地址上，也就意味着所有的invalidate/read/write都不再在本地bus了。这是一个很严重的问题。因为QPI的性能比bus要遭的多的多。
所以我们就得再做改进，让每个thread预先（compile time）就知道自己会spin在哪块地址上，从而使得每个thread都只spin在本地内存上。这个结果就是software queue lock。

## Linux RW Spinlocks

* 低24位用于表示当前的有效的readers
* 第25位用于表示writer

当有一连串的reader在读数据，writer会存在饥饿问题，应该让writer优先。Linux中的Seqlocks就是写者优先，读者可能会被饿死。

Writer侧:

1. 一个显式的write lock，同时只允许一个writer
2. 一个版本号，在进入临界区和退出临界区的时候都会对这个版本号进行递增

很显然，如果临界区没有任何writer，那么version一定是奇数，否则一定是偶数

Reader侧:
1. 获取version，如果是偶数则进去临界区，如果是奇数就等待writer 离开临界区
2. 进入临界区，读取数据
3. 获取version值，如果是初始获取的值相等就OK，否则就回到第一步。

seqlock一般适用于:
1. read操作比较频繁
2. writer操作比较少，但是性能要求高，不希望被reader thread阻挡