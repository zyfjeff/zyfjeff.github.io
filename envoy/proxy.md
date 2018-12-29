## 实现一个Proxy需要考虑的问题?
1. Downstream来了一个非法请求如何处理?
2. Downstream半关闭如何处理?
3. Downstream连接异常断开如何处理?
4. Downstream正常关闭如何处理?


1. Downstream链接关闭、所有的ActiveMessage被reset，添加到deferdelete队列中，
2. ActiveMessage析构，调用所有filter的onDestory方法，调用到route filter的onDestory
3. route filter的onDestory开始对`upstream_request_` 进行resetStream，因为Downstream已经被关闭了，所有正在执行的`upstream_request_`都要被关掉
，但是这存在一个问题，如果并不是Downstream被关闭的这种情况的，onDestory如何区分呢? 如果Downstream并不是被关闭了，而是正常的ActiveMessage析构，这会导致
当前正在服务的请求被reset掉。
4. 如果正好是一个`upstream_request_`刚结束，`upstream_request_`会被reset掉，所以onDestory的时候并不会关掉
