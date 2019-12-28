
## 为什么unix socket比tcp socket性能好?

## 为什么要做字节对齐?


## 为什么 TCP 建立连接需要三次握手?


```
The reliability and flow control mechanisms described above require that TCPs initialize and maintain certain status information for each data stream.
The combination of this information, including sockets, sequence numbers, and window sizes, is called a connection.
```

用于保证可靠性和流控制机制的信息，包括 Socket、序列号以及窗口大小叫做连接
所以，建立 TCP 连接就是通信的双方需要对上述的三种信息达成共识，连接中的一对 Socket 是由互联网地址标志符和端口组成的，窗口大小主要用来做流控制，最后的序列号是用来追踪通信发起方发送的数据包序号，接收方可以通过序列号向发送方确认某个数据包的成功接收。

为什么我们需要通过三次握手才可以初始化 Sockets、窗口大小、初始序列号并建立 TCP 连接?

* 通过三次握手才能阻止重复历史连接的初始化；
* 通过三次握手才能对通信双方的初始序列号进行初始化；
* 讨论其他次数握手建立连接的可能性；

这几个论点中的第一个是 TCP 选择使用三次握手的最主要原因，其他的几个原因相比之下都是次要的原因，我们在这里对它们的讨论只是为了让整个视角更加丰富，通过多方面理解这一有趣的设计决策。


https://draveness.me/whys-the-design-tcp-three-way-handshake