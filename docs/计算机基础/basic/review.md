
# 面试题


## 基础

* epoll是同步还是异步
* epoll 和 select模型的差异
* 如何排查内存泄露问题，给一些思路?
* epoll鲸群问题(多线程共享epoll fd)
* Linux虚假唤醒、条件变量
* 为什么要字节对齐
* 如何停止一个线程
* mmap的原理
* 常见的线程模型
* 什么是伪共享?
* 什么是内存屏障?

## C++

* 移动语义、右值引用和转发引用(通用引用)、完美转发
* 使用过哪些智能指针，shared_ptr 线程安全吗?
* volitate
* 实现线程安全的单例

## Go

* 用过那些golang的并发模式?
* [什么是逃逸分析?](https://github.com/lifei6671/interview-go/blob/master/question/q019.md)
* 基于index和基于key/value的两种for range的区别
* 字符串转成byte数组，会发生内存拷贝吗？
* [对已经关闭的的chan进行读写，会怎么样？为什么？](https://github.com/lifei6671/interview-go/blob/master/question/q018.md)


https://github.com/tmrts/go-patterns

## Envoy部分
1. Envoy的线程模型、dispatcher机制，如何保证配置更新是线程安全的?
2. Envoy的热重启机制的流程，如何处理已有连接的close
3. Envoy的内存优化思路
4. Envoy的线程池存在什么问题?

## 自己准备面试

1. vector的增长因子为什么是成倍增长，为什么是2倍(gcc是2倍，MSVC是1.5、folly的FBVector 是1.5)
有几个原因，主要围绕尽可能减少分配内存的次数、还需要保证不能浪费内存空间，最好还能充分利用缓存(就是重复利用之前回收的内存)

1、2、4、8、16、32(每次分配都没办法复用之前释放的内存，都不够)
1、1.5、2.25、3.375、5.0625、7.59375(可以复用前四次释放的内存了)，按照1.5倍增长对缓存更友好

Ref:
* [FBVector](https://github.com/facebook/folly/blob/master/folly/docs/FBVector.md)


2. HashTable的实现

核心计数点: 载入因子、解决hash冲突(链表法、重hash法)、内存消耗、缓存友好性等几个方面来分析
载入因子决定了HashTable的存在hash冲突的概率、遇到hash冲突通常的解决办法

3. 负载均衡算法

## 面试记录
