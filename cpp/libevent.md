## Libevent Note

1. 创建和释放`event_base`

* `event_base_new` 用于创建一个`event_base`结构，`event_base`就等同于一个`EventLoop`，如果不满足默认创建的`event_base`，
想对其进行一些参数的调整可以通过`event_base_new_with_config`来创建`event_base`，如果一个进程进行了fork
要想fork后的event_base仍然有效，需要调用`event_reinit`来进行重新初始化。

* `event_base_free` 释放`event_base`的结构，但是不是释放和这个`event_base`所关联的事件和套接字等。

2. `event_base`查询

* `event_get_supported_methods` 返回libevent在当前OS上支持的事件机制的类型
* `event_base_get_method` 返回当前`event_base`使用的事件机制类型
* `event_base_get_features` 返回当前`event_base`支持的features

3. `event_base`优先级

* `event_base_priority_init` 用户初始化`event_base`支持的优先级数量，默认只支持单个优先级，这个函数需要在`event_base`创建后立刻执行
常量`EVENT_MAX_PRIORITIES`表示的则是事件的最大优先级数量，创建事件的时候可以给事件指定优先级。

* `event_priority_set` 设置事件的优先级

> 如果不为事件设置优先级,则默认的优先级将会是 event_base 的优先级数目除以2。

4. EventLoop运行

* `event_base_loop` 用于运行事件循环，有三种模式，分别为`EVLOOP_NO_EXIT_ON_EMPTY`、`EVLOOP_NONBLOCK`、`EVLOOP_ONCE`

```
while (any events are registered with the loop,
        or EVLOOP_NO_EXIT_ON_EMPTY was set) {

    if (EVLOOP_NONBLOCK was set, or any events are already active)
        If any registered events have triggered, mark them active.
    else
        Wait until at least one event has triggered, and mark it active.

    for (p = 0; p < n_priorities; ++p) {
       if (any event with priority of p is active) {
          Run all active events with priority of p.
          break; /* Do not run any events of a less important priority */
       }
    }

    if (EVLOOP_ONCE was set or EVLOOP_NONBLOCK was set)
       break;
}
```

EVLOOP_NONBLOCK和EVLOOP_ONCE的区别在于前者并不循环等待事件触发，循环仅仅直接检测是否有事件已经就绪，有的话就立即触发，然后执行事件回调
后者会循环等待事件触发，然后激活事件执行事件回调。

* `event_base_dispatch` 等同于 `event_base_loop`没有加任何标志，会一直运行，直到没有任何注册的事件。
或者调用`event_base_loopbreak`和`event_base_loopexit`为止

5. 停止EventLoop

* `event_base_loopexit` 让`event_base`在给定时间之后停止循环。如果`tv`参数为 NULL, `event_base`会立即停止循环,没有延时。
* `event_base_loopbreak` 让`event_base`立即退出循环。它与`event_base_loopexit(base,NULL)`的不同在于,如果`event_base`当前正在执行激活事件的回调 ,它将在执行完当前正在处理的事件后立即退出。
* `event_base_got_exit` 判断退出循环的原因是否是因为调用`event_base_loopexit`导致的
* `event_base_got_break` 判断退出循环的原因是否是因为调用`event_base_loopbreak`导致的

6. event事件

一个事件创建后被关联到`event_base`中后就变成了已初始化的状态，然后将事件添加到`event_base`中就会使事件进入未决状态，在未决状态下如果事件状态发生了变化，就会进行激活状态。
事件的回调函数就会被执行，执行完成后事件就不再是未决状态，需要重新添加到`event_base`中，可以设置事件为`persistent`的，那么事件就会始终保持是未决的，不用每次都添加到`event_base`中

> 每个进程任何时刻只能有一个`event_base`可以监 听信号。如果同时向两个`event_base`添加信号事件,即使是不同的信号,也只有一个`event_base`可以取得信号。

* `event_base_dump_events` 将当前`event_base`中的事件完整的dump出来
* `event_new` 创建一个事件
* `event_free` 释放一个事件
* `evsignal_new` 本质上就是`event_new`结合EV_SIGNAL的标志
* `event_base_once` 创建一个一次性事件，事件触发后执行完回调后会释放`event`结构
* `event_active` 激活事件(事件不需要已经处于未决状态,激活事件也不会让它成为未决的)

> 不能删除或者手动激活使用`event_base_once`插入的事件:如果希望能够取消事件, 应该使用`event_new`或者`event_assign`。

```
#define evsignal_new(base, signum, cb, arg) \
    event_new(base, signum, EV_SIGNAL|EV_PERSIST, cb, arg)
```

* `evtimer_new`
* `event_add` 使事件变成未决状态，在未决状态下事件的状态发生变化就会触发`event_base`返回
* `event_del` 将事件设置成非未决状态

7. event事件标志

* EV_TIMEOUT 超时事件
* EV_READ    可读事件
* EV_WRITE   可写事件
* EV_SIGNAL  信号事件
* EV_PERSIST 始终让事件保持未决状态
* EV_ET      边缘触发

8. 事件状态查询

* `event_get_signal(ev)`   获取事件注册的信号值
* `event_get_fd`           获取事件对应的fd
* `event_get_base`         获取事件对应的event_base
* `event_get_events`       获取事件对应的事件标志
* `event_get_callback`     获取事件对应的callback
* `event_get_callback_arg` 获取事件对应的callback参数
* `event_get_priority`     获取事件对应的优先级
* `event_get_assignment`   可以一次性获取事件所有的状态信息

9. bufferevent和evbuffer
bufferevent有四个水位(读取低水位、读取高水位、写入低水位、写入高水位)

BEV_OPT_CLOSE_ON_FREE
BEV_OPT_THREADSAFE
BEV_OPT_DEFER_CALLBACKS
BEV_OPT_UNLOCK_CALLBACKS

* `bufferevent_socket_new`
* `bufferevent_socket_connect`
* `bufferevent_setcb`
* `bufferevent_free`
* `bufferevent_getcb`
* `bufferevent_enable`
* `bufferevent_disable`
* `bufferevent_get_enabled`
* `bufferevent_setwatermark`

> `bufferevent`内部具有引用计数,所以,如果释放时还有未决的延迟回调,则在回调完成之前`bufferevent`不会被删除。