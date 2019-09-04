三次握手的过程中，当用户首次访问server时，发送`syn`包，server根据用户IP生成`cookie`，并与`syn+ack`一同发回client，client再次访问server时，在syn包携带TCP cookie；
如果server校验合法，则在用户回复ack前就可以直接发送数据；否则按照正常三次握手进行。TFO提高性能的关键是省去了热请求的三次握手，这在充斥着小对象的移动应用场景中能够极大提升性能。
客户端使用起来比较麻烦如果使用`sendmsg/sendto`系统调用，需要加上`MSG_FASTOPEN`这个flag，服务端不需要做任何设置。

<img src="../images/fastopen.jpg" width = "1000" height = "800" alt="archite" align="center" />

#### Reference

* [TCP Fast Open: expediting web services](https://lwn.net/Articles/508865/)
* [TCP Fast Open Internals](https://www.atatech.org/articles/71231?flag_data_from=)
