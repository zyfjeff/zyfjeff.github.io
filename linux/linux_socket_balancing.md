
## 三种常见的socket balancing模式

1. Single listen socket, single worker process.


```
Network stack -> Accept queue -> Worker
```


2. Single listen socket, multiple worker processes.

```

                                -> Worker 1
Network stack -> Accept queue   -> Worker 2
                                -> Worker 3
```

3. Multiple worker processes, each with separate listen socket.

```
              -> Accept queue1   -> Worker 1
Network stack -> Accept queue2   -> Worker 2
              -> Accept queue3   -> Worker 3
```

方案2中通过阻塞的Accept来接收连接会将所有的连接平均分配给多个Worker，类似于RR轮询，遵循FIFO模型，但是如果使用epoll针对非阻塞的Accept则会导致大部分的连接都被分配到一个worker中,遵循的是LIFO模型
会导致连接分布不均衡，可以通过SO_REUSEPORT创建多个listen socket，也就是上面的方案3，虽然这个方案可以使得各个worker进程负载均衡，但是会导致延迟变大

> https://blog.cloudflare.com/the-sad-state-of-linux-socket-balancing/


epoll的鲸群问题:

The best and the only scalable approach is to use recent Kernel 4.5+ and use level-triggered events with EPOLLEXCLUSIVE flag. This will ensure only one thread is woken for an event, avoid "thundering herd" issue and scale properly across multiple CPU's


Without EPOLLEXCLUSIVE, similar behavior it can be emulated with edge-triggered and EPOLLONESHOT, at a cost of one extra epoll_ctl() syscall after each event. This will distribute load across multiple CPU's properly, but at most one worker will call accept() at a time, limiting throughput.