* enable SO_REUSEADDR

* ignore SIGPIPE in any concurrent TCP server

往已经关闭的socket写数据的时候会触发SIGPIPE信号

* TCP_NODELAY Nagle's algorithm

Nagle算法的基本定义是任意时刻，最多只能有一个未被确认的小段。 所谓“小段”，指的是小于MSS尺寸的数据块，
所谓“未被确认”，是指一个数据块发送出去后，没有收到对方发送的ACK确认该数据已收到。

  1. 如果包长度达到MSS，则允许发送；
  2. 如果该包含有FIN，则允许发送；
  3. 设置了TCP_NODELAY选项，则允许发送；
  4. 未设置TCP_CORK选项时，若所有发出去的小数据包（包长度小于MSS）均被确认，则允许发送；
  5. 上述条件都未满足，但发生了超时（一般为200ms），则立即发送。

  影响响应的延迟、调用writer后如果没有收到上一次的ACK就不发送数据。

* SO_LINGER


* The correct close the socket

正常情况下如果接收缓冲区有数据，这个时候关闭socket会导致RST分节发送，如果此时发送缓冲区中有数据会导致数据丢失。

正确关闭socket的方法:

  1. 发送方: send() + shutdown(WR) + read()->(超时控制) + close()
  2. 接收方: read()->0 + if nothing more to send + close()