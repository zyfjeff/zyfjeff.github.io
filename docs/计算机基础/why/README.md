# 计算机基础百科

## 什么是虚假唤醒? 需要长期wait的系统调用为什么会被signal打断?

虚假唤醒就是一些阻塞的调用被唤醒，但是实际上并没有达到预期的唤醒条件，比如读取socket的时候一直阻塞的等待数据到来，但是实际上
并没有数据也导致了read返回，还有条件变量在wait的时候，可能条件未满足也导致了wait返回。出现虚假环境的原因，总结来说有三点:


1. 多线程多处理器情况下导致的条件变量唤醒多次

    * http://pubs.opengroup.org/onlinepubs/000095399/functions/pthread_cond_signal.html

    Following is the quote from the above link for code snippet:

    ```C++
    pthread_cond_wait(mutex, cond):
        value = cond->value; /* 1 */
        pthread_mutex_unlock(mutex); /* 2 */
        pthread_mutex_lock(cond->mutex); /* 10 */
        if (value == cond->value) { /* 11 */
            me->next_cond = cond->waiter;
            cond->waiter = me;
            pthread_mutex_unlock(cond->mutex);
            unable_to_run(me);
        } else
            pthread_mutex_unlock(cond->mutex); /* 12 */
        pthread_mutex_lock(mutex); /* 13 */


    pthread_cond_signal(cond):
        pthread_mutex_lock(cond->mutex); /* 3 */
        cond->value++; /* 4 */
        if (cond->waiter) { /* 5 */
            sleeper = cond->waiter; /* 6 */
            cond->waiter = sleeper->next_cond; /* 7 */
            able_to_run(sleeper); /* 8 */
        }
        pthread_mutex_unlock(cond->mutex); /* 9 */
    ```

    线程A执行上上面代码中的步骤2，线程B在另外一个CPU上运行，执行`pthread_cond_signal`，线程C已经阻塞在`pthread_cond_wait`调用中了，这个时候就会出现`pthread_cond_signal`唤醒了A和C

    * https://sites.google.com/site/embeddedmonologue/home/mutual-exclusion-and-synchronization/spurious-wakeup-and-infinite-waiting-on-condvar

2. 因为OS调度导致

    同样在单线程情况下，线程A运行到步骤2后可能因为优先级或者时间片用完等原因，切回到线程B运行`pthread_cond_signal`从而导致唤醒了多个线程

3. 被信号中断

    Linux系统中，阻塞的慢系统调用会被信号打断，并设置对应的错误码为EINTR，导致虚假唤醒


## 什么是异步信号安全?

## 为什么unix socket比tcp socket性能好?


## 为什么Linux中的系统调用会返回EINTR错误?

这个问题可能大多数人都只是知道要怎么去处理，但是却不知道为什么会发生中断? 如果一个系统调用在执行的时候，一个信号到来，这个时候需要切换到信号处理函数中执行，
但是系统调用是在内核态中执行的，如果现在执行以内核态来运行用户态编写的信号处理函数，这会给内核带来安全风险，所以理论上这里应该要切换运行态岛用户态，
既然要切换那么就需要保存当前的状态，但是这就涉及到一个问题就是中间状态，这个时候执行信号处理函数可能看到一个中间的无效状态，导致程序行为时未定义的，
还有锁的问题，资源占用等问题。所以这种切换状态的方式是不可行的，linux使用了一种更加灵活的方式来处理，当系统调用在执行的过程中有信号到来则中断当前的系统调用，
然后返回-1，并设置错误码为EINTR，交给用户来重新再运行该系统调用。

* https://lwn.net/Articles/17744/
* https://unix.stackexchange.com/questions/16455/interruption-of-system-calls-when-a-signal-is-caught
* https://blog.csdn.net/zhangyifei216/article/details/77955339


## 为什么要做字节对齐?

## x86_64处理器的指针赋值是原子操作吗？

https://blog.csdn.net/dog250/article/details/103948307

## 为什么 TCP 建立连接需要三次握手?

!!! Note
    The reliability and flow control mechanisms described above require that TCPs initialize and maintain certain status information for each data stream.
    The combination of this information, including sockets, sequence numbers, and window sizes, is called a connection.


用于保证可靠性和流控制机制的信息，包括 Socket、序列号以及窗口大小叫做连接，所以，建立 TCP 连接就是通信的双方需要对上述的三种信息达成共识，连接中的一对 Socket 是由互联网地址标志符和端口组成的，
窗口大小主要用来做流控制，最后的序列号是用来追踪通信发起方发送的数据包序号，接收方可以通过序列号向发送方确认某个数据包的成功接收。

为什么我们需要通过三次握手才可以初始化 Sockets、窗口大小、初始序列号并建立 TCP 连接?

* 通过三次握手才能阻止重复历史连接的初始化；
* 通过三次握手才能对通信双方的初始序列号进行初始化；
* 讨论其他次数握手建立连接的可能性；

这几个论点中的第一个是 TCP 选择使用三次握手的最主要原因，其他的几个原因相比之下都是次要的原因，我们在这里对它们的讨论只是为了让整个视角更加丰富，通过多方面理解这一有趣的设计决策。

https://draveness.me/whys-the-design-tcp-three-way-handshake