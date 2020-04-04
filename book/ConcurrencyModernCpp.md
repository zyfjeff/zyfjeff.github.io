
## what is a memory model?

> The C++ memory model defines a contract, this contract is between the programmer and the system
> The system consists of the compiler that generates machine code, the processor that executes the machine
> code and includes the different caches that store the state of the program. Each of the participants wants to optimise its part
> with the strong memory model I refer to sequential consistency, and with the weak memory model I refer to relaxed semantic.

1. 内存模型定义了程序员和系统(包含编译器、处理器、Cache)的约定，有了这些约定，系统才可以尽可能的做各种优化。
2. 强内存模型对应顺序一致性、弱内存模型泽对应松散语义


Sequential consistency provides two guarantees:
• The instructions of a program are executed in source code order.
• There is a global order of all operations on all threads.

顺序一致性提供了两个保证:

1. 程序的指令执行是按照代码中的顺序来的
2. 所有的操作在所有的线程中的都是全局有序的。


`std::atomic_flag` has two outstanding properties.
`std::atomic_flag` is

• the only lock-free atomic. A non-blocking algorithm is lock-free if there is guaranteed system- wide progress.
• the building block for higher level thread abstractions.

The only lock-free atomic? The remaining more powerful atomics can provide their functionality by using a mutex internally according to the C++ standard. These remaining atomics have a method called is_lock_free to check if the atomic uses a mutex internally. On the popular microprocessor architectures, I always get the answer true. You should be aware of this and check it on your target system if you want to program lock-free.

`std::atomic_flag  != std::atomic<bool>`

Condition variables may be victim to two phenomena:

1. spurious wakeup: the receiver of the message wakes up, although no notification happened
2. lost wakeup: the sender sends its notification before the receiver gets to a wait state.



There are three different kinds of operations:
• Readoperation:memory_order_acquire and memory_order_consume
• Write operation: memory_order_release
• Read-modify-write operation:memory_order_acq_rel and memory_order_seq_cst

memory_order_relaxed defines no synchronisation and ordering constraints. It does not fit in this taxonomy.
The default for atomic operations is std::memory_order_seq_cst

六种原子操作的memory model，可以分为三类，不能随便用，否则意义不大，比如

load 属于read，但是如果给他添加memory_order_release则没有任何意义，等同于memory_order_relaxed，如果添加的是memory_order_acq_rel，它是属于
Read-modify-write类型，所以writer部分就没有效果了。等同于memory_order_acquire。

总体上分为三类:

• Sequential consistency: memory_order_seq_cst
• Acquire-release: memory_order_consume, memory_order_acquire, memory_order_release, and
memory_order_acq_rel
• Relaxed:memory_order_relaxed

While the sequential consistency establishes a global order between threads, the acquire-release semantic establishes an ordering between reading and writing operations on the same atomic variable with different threads. The relaxed semantic only guarantees the modification order of some atomic m. Modification order means that all modifications on a particular atomic m occur in some particular total order.Consequently, reads of an atomic object by a particular thread never see “older” values than those the thread has already observed.

顺序一致性保证了在线程间是全局有序的，acquire-release语义则建立对于相同的原子变量的的不同线程之间的read和write操作是有序的。relaxed语义只保证某些原子m的修改顺序。修改顺序意味着对特定原子m的所有修改都以特定的总顺序发生。因此，特定线程对原子对象的读取永远看不到这个线程已经观察到的“旧”值。




* wait-free 和 lock-free的区别

* 小心map的[]索引访问，当key不存的时候，实际上会创建一个默认的值，这是一个写操作。小心会产生data race


* Fire and Forget
Fire and forget futures look very promising but have a big drawback. A future that is created by std::async waits on its destructor, until its promise is done. In this context, waiting is not very different from blocking. The future blocks the progress of the program in its destructor. This becomes more evident, when you use fire and forget futures. What seems to be concurrent actually runs sequentially.

```cpp
#include<chrono>
#include<future>
#include<iostream>
#include<thread>
int main(){
  std::cout << std::endl;
  std::async(std::launch::async, []{
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "first thread" << std::endl;
  });

  std::async(std::launch::async, []{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "second thread" << std::endl;}
  );

  std::cout << "main thread" << std::endl; std::cout << std::endl;
}
```


* ThreadSanitizer `-fsanitize=thread`



Lock free:

1. Hazard Pointers
2. RCU
3. fast latching
