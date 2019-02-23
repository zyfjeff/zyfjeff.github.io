## How TCP backlog works in Linux

Linux有两个队列，一个是半连接队列，另外一个是连接队列，半连接队列的大小由[1]控制，连接队列的大小是程序控制的程序在listen的时候需要传入一个backlog值，这个值就是连接队列的大小，程序accept的时候其实就是从这个连接队列中取一个连接出来。典型的一次连接过程如下:

1. 客户端发出syn请求， 状态由CLOSED变成SYN_SEND
2. 服务端收到syn请求后，将该请求放到半连接队列，然后返回syn + ack，状态变为SYN_RECV
3. 客户端受到syn+ack后，响应一个ack请求，状态变为ESTABLISHED
4. 服务器端收到ack请求后，将该连接从半连接队列移动到连接队列中，状态变为ESTABLISHED

如果连接队列是满的，那么 服务器端会忽略客户端发过来的ack请求，那么客户端会进行重发最后一个ack请求，重发的次数取决于[2]如果在重发过程中服务器端的连接队列不是满了，那么就会响应重发的ack，将连接从半连接队列移动到请求队列中，否则连接就断掉了处于半打开的状态，从服务器端是无法看到这个连接的，但是在客户端看来这个连接是ESTABLISHED状态的。对于这个问题可以借助于[3]将这个选项设置为1的时候，如果服务器端的连接队列是满的，那么客户端发起连接的时候会收到服务器端响应的RST报文。


* [1] /proc/sys/net/ipv4/tcp_max_syn_backlog
* [2] /proc/sys/net/ipv4/tcp_synack_retrie
* [3] /proc/sys/net/ipv4/tcp_abort_on_overflow
