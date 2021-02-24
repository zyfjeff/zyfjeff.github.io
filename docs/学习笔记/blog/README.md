
# Can Reordering of Release/Acquire Operations Introduce Deadlock?


* `read-acquire`


```cpp
int b = B.load(std::memory_order_acquire);
```

任何在这条语句后的读写操作都不能进行重排序

A read-acquire operation cannot be reordered, either by the compiler or the CPU, with any read or write operation that follows it in program order

* `write-release`

```cpp
B.store(1, std::memory_order_release);
```

任何在这条语句前的读写操作都不能进行重排序

A write-release operation cannot be reordered with any read or write operation that precedes it in program order.


总结: 读后(acquire后的操作无法重排序)、写前(release前的操作无法重排序)

但是如果是下面这种形式，则不保证是否要进行重排序:

```cpp
A.store(1, std::memory_order_release);
int b = B.load(std::memory_order_acquire);

// 可能会发生重排序，重排序后的结果如下，

int b = B.load(std::memory_order_acquire);
A.store(1, std::memory_order_release);
```



假设有这样的case:

```cpp

// Thread 1
int expected = 0;
while (!A.compare_exchange_weak(expected, 1, std::memory_order_acquire)) {
    expected = 0;
}

// 这条语句可能会重排序到B.compare_exchange_weak之后
// Unlock A
A.store(0, std::memory_order_release);

// Lock B
while (!B.compare_exchange_weak(expected, 1, std::memory_order_acquire)) {
    expected = 0;
}

// A.store(0, std::memory_order_release); 被重排序到这
// Unlock B
B.store(0, std::memory_order_release);


// Thread 2
// Lock B
int expected = 0;
while (!B.compare_exchange_weak(expected, 1, std::memory_order_acquire)) {
    expected = 0;
}

// Lock A
while (!A.compare_exchange_weak(expected, 1, std::memory_order_acquire)) {
    expected = 0;
}

// Unlock A
A.store(0, std::memory_order_release);

// Unlock B
B.store(0, std::memory_order_release);
```

如果`Thread1`中发生了重排序就会和`Thread2`发生死锁，


[C++标准中的4.7.2:18中提到](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/n4659.pdf):

```
An implementation should ensure that the last value (in modification order)
assigned by an atomic or synchronization operation will become visible to all other threads in a finite period of time.
```

```C++
// Unlock A
A.store(0, std::memory_order_release);

// Lock B
while (!B.compare_exchange_weak(expected, 1, std::memory_order_acquire)) {
    expected = 0;
}
```

针对上面的场景来看，当我们执行到while循环的时候，A已经被赋值为0，C++标准中说到最后被修改的值应该在一个有限的时间内
被其他线程看到，但是这里的while loop如果是无限循环呢? 编译器无法排除这种情况，因此，如果编译器不能排除while循环是无限的，
那么它就不能把`A.store`进行重排序到while循环之后。如果进行了重排序就违反了C++标准。

因此对于上述的这种情况是不会发生死锁的，借助于[Matt Godbolt’s Compiler Explorer](https://godbolt.org/g/DW6fuV)可以验证上述代码在不同编译器
的优化编译场景下是否发生了重排序。事实上主流的C++编译器都没有发生重排序。


[Can Reordering of Release/Acquire Operations Introduce Deadlock?](https://preshing.com/20170612/can-reordering-of-release-acquire-operations-introduce-deadlock/)


