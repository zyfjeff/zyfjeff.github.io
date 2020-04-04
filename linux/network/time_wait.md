## 为什么要有`TIME_WAIT`状态?

![duplicate-segment](../images/duplicate-segment.png)

* 避免串话，上图中一端连续发送了三个包，其中`SEQ=3`的包`delay`了还没有到达对端，这个时候如果对端关闭连接，并发起四次挥手的过程，最后停留在`TIME-WAIT`状态，假想下如果没有`TIME-WAIT`状态，这个时候对端重新发起一个新的链接并且使用的端口和之前一样，连接建立后好开始正常的数据传输，这个时候`SEQ=3`包到达了，并且序号正好连续，这个时候就会被当成是正确的包来接收。

![last-ack](../images/last-ack.png)

* 一端发起`FIN`四次挥手，在发送完最后一个`ACK包`的时候，因为网络等问题导致这个包迟迟没有达到对端，使得对端一直处于`LAST-ACK`的状态，这个时候如果没有`TIME-WAIT`状态，那么对端就可以重新发起一个新的请求，这个请求达到对端的时候，因为对端还处于`LAST-ACK`状态，导致对端响应一个RST包文。

总结来说有几点

1. 只有主动发起四次挥手的一方才会进入`TIME-WAIT`状态，这个角色通常是服务端。

2. `TIME_WAIT`状态下相同的端口无法再次发起连接请求，调用`connect`会返回`EADDRNOTAVAL`。

3. 处于`TIME_WAIT`状态下的`socket`内核对象会一直保留在内核中，占用少量的内存资源。

4. 客户端一般通过增加端口范围、配置多个IP、设置`net.ipv4.tcp_tw_reuse`选项的方式来规避因为`TIME-WAIT`状态导致的无法连接服务的问题。

5. 通过关闭`socket lingering`可以避免进入`TIME-WAIT`状态，直接以`RST`报文响应。

6. `net.ipv4.tcp_tw_reuse`选项通过比较`socket`的`timestamp`字段，如果字段是递增的且大于1秒就复用`TIME-WAIT`状态的`socket`，只针对那些`对外连接`的`socket`对象。(**很重要，对外连接的socket的才起作用**)

7. ``net.ipv4.tcp_tw_reuse``可以有效避免串话，因为延迟到来的包，它的时间戳小于新创建的连接，会直接`RST`报文响应(如下图)。

   ![Last ACK lost and timewait reuse](../images/last-ack-reuse.svg)

8. `net.ipv4.tcp_tw_recycle`的原理也是利用了`timestamp`，但是对进来的连接和对外连接的socket都起作用。对于对外连接的socket原理和`net.ipv4.tcp_tw_reuse`一致，对于进来的连接在指定时间范围内要求`timestamp`递增，否则就响应RST报文。

   ```cpp
   #define TCP_PAWS_WINDOW 1
   #define TCP_PAWS_MSL    60

   if (tmp_opt.saw_tstamp &&
       tcp_death_row.sysctl_tw_recycle &&
       (dst = inet_csk_route_req(sk, &fl4, req, want_cookie)) != NULL &&
       fl4.daddr == saddr &&
       (peer = rt_get_peer((struct rtable *)dst, fl4.daddr)) != NULL) {
           inet_peer_refcheck(peer);
       	// 查看是否超时，
           if ((u32)get_seconds() - peer->tcp_ts_stamp < TCP_PAWS_MSL &&
               (s32)(peer->tcp_ts - req->ts_recent) >
                                           TCP_PAWS_WINDOW) {
                   NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_PAWSPASSIVEREJECTED);
                   goto drop_and_release;
           }
   }
   ```

   对于第一个条件`(u32)get_seconds() - peer->tcp_ts_stamp < TCP_PAWS_MSL`这个要求在60秒内相同的源地址和端口发起的请求，`(s32)(peer->tcp_ts - req->ts_recent) > TCP_PAWS_WINDOW`第二个条件就是要求时间戳是递增的，只有同时满足这两个条件才会被认为是有效的，就可以复用`TIME_WAIT`的socket，否则就响应RST报文。在NAT场景下，NAT背后的一堆机器其时钟并不是一致的，所以一定概率的情况下会出现前后两个链接，后者的timestamp要大于前者，结果就会导致后面的链接被RST掉，导致问题的发生。

9. `net.ipv4.tcp_tw_recyle`和`net.ipv4.tcp_tw_reuse`同时开启是无用的，客户端一般开启`net.ipv4.tcp_tw_reuse`即可。

#### Reference

* [Coping with the TCP TIME-WAIT state on busy Linux servers](https://vincent.bernat.im/en/blog/2014-tcp-time-wait-state-linux)
