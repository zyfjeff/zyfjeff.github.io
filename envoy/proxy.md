# 线程模型

单进程多线程架构，一个主线程控制各种零星的协调任务，多个worker线程执行listening、filtering、和数据转发等，只要一个连接被listener接受，那么这个连接上
所有的工作都是在一个线程中完成的。

# 性能优化

1. `--disable-hot-restart` 关闭热重启，避免使用共享内存存放stats统计信息
2. `--concurrency` 和绑定的核心数一致，避免多个CPU切换导致cache失效
3. 关闭stats统计和添加自定义header
```
http_filters:
- name: envoy.router
  config:
      dynamic_stats: false
      suppress_envoy_headers: false
```
4. 关闭circuit_breakers

```
circuit_breakers:
  thresholds:
  - priority: HIGH
    max_connections: 1000000000
    max_pending_requests: 1000000000
    max_requests: 1000000000
    max_retries: 1000000000
```

## 硬件篇
AVX2指令集支持，


# listener filter
作用在listen socket上，当有连接到来的时候，通过libevent会触发可读事件，调用listen socket的accept获取到连接socket封装为ConnectionSocket，
最后调用ActiveListener::onAccept，将获取到的连接socket作为其参数。

1. 创建filter chain
2. continueFilterChain 调用filter chain
3. 如果有filter返回了StopIteration，那么就开启timer，在这个时间内还没有继续continue就直接关闭当前socket

```cpp
void ConnectionHandlerImpl::ActiveListener::onAccept(
    Network::ConnectionSocketPtr&& socket, bool hand_off_restored_destination_connections) {
  auto active_socket = std::make_unique<ActiveSocket>(*this, std::move(socket),
                                                      hand_off_restored_destination_connections);

  // Create and run the filters
  config_.filterChainFactory().createListenerFilterChain(*active_socket);
  active_socket->continueFilterChain(true);

  // Move active_socket to the sockets_ list if filter iteration needs to continue later.
  // Otherwise we let active_socket be destructed when it goes out of scope.
  if (active_socket->iter_ != active_socket->accept_filters_.end()) {
    active_socket->startTimer();
    active_socket->moveIntoListBack(std::move(active_socket), sockets_);
  }
}
```

目前实现的listener filter主要有`original_dst`、`original_src`、`proxy_protocol`、`tls_inspector`等

## original_dst filter
一般应用于通过iptables或者tproxy的方式将流量发送给envoy，导致原来要访问的地址信息丢失，但是可以通过从socket中获取到这些信息，交给envoy做listen转发。

1. 主要就是从socket中获取到原来的目的地址信息 (`getsockopt(fd, SOL_IP, SO_ORIGINAL_DST, &orig_addr, &addr_len)`)
2. 然后设置socket的restore_local_address为原来的目的地址

```cpp
Network::FilterStatus OriginalDstFilter::onAccept(Network::ListenerFilterCallbacks& cb) {
  ENVOY_LOG(debug, "original_dst: New connection accepted");
  Network::ConnectionSocket& socket = cb.socket();
  const Network::Address::Instance& local_address = *socket.localAddress();

  if (local_address.type() == Network::Address::Type::Ip) {
    Network::Address::InstanceConstSharedPtr original_local_address =
        getOriginalDst(socket.ioHandle().fd());

    // A listener that has the use_original_dst flag set to true can still receive
    // connections that are NOT redirected using iptables. If a connection was not redirected,
    // the address returned by getOriginalDst() matches the local address of the new socket.
    // In this case the listener handles the connection directly and does not hand it off.
    if (original_local_address) {
      // Restore the local address to the original one.
      socket.restoreLocalAddress(original_local_address);
    }
  }

  return Network::FilterStatus::Continue;
```
Reference: https://www.kernel.org/doc/Documentation/networking/tproxy.txt



## original_src filter


## data sharing between filters

Network level filters can also share state (static and dynamic) among themselves within the context of a single downstream connection

1. Static State

不可变的状态，指的是配置加载的时候指定的一些状态信息，主要有三类Static State

* Metadata

  在Envoy的配置中有部分字段会包含metadata信息，比如listeners, routes, clusters等，对应到Protobuf就是

  ```cpp
    message Metadata {
      // Key is the reverse DNS filter name, e.g. com.acme.widget. The envoy.*
      // namespace is reserved for Envoy's built-in filters.
      map<string, google.protobuf.Struct> filter_metadata = 1;
    }
  ```
  比如集群的权重、负载均衡中的subset metadata信息等

* Typed Metadata
  Metadata是无类型，因此在使用之前需要将其转换为内部定义的对象，每次新建连接或者新的请求的时候，都需要重复执行，带来的开销是不小的。
  Typed Metadata就是用来解决这个问题，可以给特定的key注册一个转换逻辑，对于输入的Metadata在配置加载的时候就会自动转换成对应的对象。
  这样就可以直接使用，避免每次获取Metadata进行转换。

  ```
  class ClusterTypedMetadataFactory : public Envoy::Config::TypedMetadataFactory {};
  class HttpRouteTypedMetadataFactory : public Envoy::Config::TypedMetadataFactory {};
  ```

* HTTP Per-Route Filter Configuration
  per_filter_config允许HTTP filters可以有自己独立的virtualhost和路由配置

2. Dynamic State
由每一个网络连接、或者是每一个HTTP stream产生，这个对象包含了当前TCP connection和HTTP stream相关的一些信息。这些信息是由固定的属性组成的，
比如HTTP的协议、请求的server name等等。此外还提供了存储typed objects的能力`map<string, FilterState::Object>`，用于给用户提供存储一些自定义信息的。
`ConnectionImpl`中包含的StreamInfo，用于在连接级别共享一些State，`Http::ConnectionManagerImpl::ActiveStream`中包含的则是per stream。


## HTTP routing
1. 基于Virtual hosts的路由
2. 前缀匹配、或者是精确匹配(大小写敏感或者不敏感都可以)，目前还不支持基于正则的匹配。
3. TLS redirection
4. Direct Response

```cpp
message DirectResponseAction {
  // Specifies the HTTP response status to be returned.
  uint32 status = 1 [(validate.rules).uint32 = {gte: 100, lt: 600}];

  // Specifies the content of the response body. If this setting is omitted,
  // no body is included in the generated response.
  //
  // .. note::
  //
  //   Headers can be specified using *response_headers_to_add* in the enclosing
  //   :ref:`envoy_api_msg_route.Route`, :ref:`envoy_api_msg_RouteConfiguration` or
  //   :ref:`envoy_api_msg_route.VirtualHost`.
  core.DataSource body = 2;
}
```
可以指定response status code和body，body可以是内联字符串或者是一个文件路径，但是都必须限制在4K以内，因为Envoy会缓存body。
5. 显示的主机名重写

```cpp
route.RouteAction:
host_rewrite: "...."
```

6. 自动主机重写

host header会被重写为选择的cluster中的名字，要求cluster需要是strict_dns或者是logical_dns类型。

```cpp
route.RouteAction:
auto_host_rewrite: "...." // 和host_rewrite二选其一
```

```cpp
void Filter::UpstreamRequest::onPoolReady(Http::StreamEncoder& request_encoder,
                                          Upstream::HostDescriptionConstSharedPtr host) {
  .......
  if (parent_.route_entry_->autoHostRewrite() && !host->hostname().empty()) {
    parent_.downstream_headers_->Host()->value(host->hostname());
  }
  .......
}
```

7. 前缀重写

```yaml
- match:
    prefix: "/prefix/"
  route:
    prefix_rewrite: "/"
- match:
    prefix: "/prefix"
  route:
    prefix_rewrite: "/"
```

第一个将`/prefix/`替换为`/`，第二个则是将`/prefix`替换为`/`，重写完成后会将之前的路径放在`x-envoy-original-path` header中。

8. 请求重试
可以基于请求中携带的header来开启重试、也可以配置一个VirtualHost级别的重试、也可以是RouteAction级别的重试。如果上游返回`x-envoy-overloaded` header
那么就关闭重试。如果upstream返回的header中带有`x-envoy-ratelimited`也要禁止重试。

9. timeout
默认的超时时间是15s，这个时间包含了从downstream request接收到end stream开始到处理到upstream response响应结束还包括了整个重试期间的时间。可以通过
`x-envoy-upstream-rq-timeout-ms` header来设置这个值。`x-envoy-upstream-rq-per-try-timeout-ms` 通过这个header来可以控制单词响应的超时时间
不包括retry。

10. 流量拆分/转移

```yaml
virtual_hosts:
   - name: www2
     domains:
     - '*'
     routes:
       - match:
           prefix: /
           runtime_fraction:
             default_value:
               numerator: 50
               denominator: HUNDRED
             runtime_key: routing.traffic_shift.helloworld
         route:
           cluster: helloworld_v1
       - match:
           prefix: /
         route:
           cluster: helloworld_v2
```

## Connection pooling
HTTP/1.1 连接池，同步的，一个请求绑定一个连接，这个连接处理完这个请求，才会变成可用状态，才能处理下一个请求，单个downstream断链只能导致一个请求出问题。
HTTP2/2 一个upstream host只会建立一条连接，上游所有的请求都会通过整条连接发送到upstream的主机，如果收到GOAWAY frame或者到达最大流限制，那么连接池
会重新创建一个新的连接，然后drain掉正在服务的连接。


## Runtime


## xDS REST and gRPC protocol

Envoy支持通过文件系统、查询管理服务器的方式获取各种类型的动态资源。这种发现服务和其对应的API被称为xDS，资源需要通过指定一个文件系统路径去watch、
或者初始化一个grpc stream或者是通过RESET-JSON URL进行pooling的方式进行订阅。后两种方式会通过发送一个`DiscoveryRequest`请求来获取资源，
管理服务器会返回`DiscoveryResponse`，其中包含了请求的资源。

目前有五种类型的资源，分别如下:

* [LDS: `envoy.api.v2.Listener`](https://github.com/envoyproxy/envoy/blob/master/api/envoy/api/v2/lds.proto)
* [RDS: `envoy.api.v2.RouteConfiguration`](https://github.com/envoyproxy/envoy/blob/master/api/envoy/api/v2/rds.proto)
* [CDS: `envoy.api.v2.Cluster`](https://github.com/envoyproxy/envoy/blob/master/api/envoy/api/v2/cds.proto)
* [EDS: `envoy.api.v2.ClusterLoadAssignment`](https://github.com/envoyproxy/envoy/blob/master/api/envoy/api/v2/eds.proto)
* [SDS: `envoy.api.v2.Auth.Secret`](https://github.com/envoyproxy/envoy/blob/master/api/envoy/api/v2/sds.proto)

每一个类型的资源都有一个URL来唯一表示，其形式为`type.googleapis.com/<resource type>`

下面是一个DiscoveryRequest的例子:
```yaml
version_info:
node: { id: envoy }
resource_names:
- foo
- bar
type_url: type.googleapis.com/envoy.api.v2.ClusterLoadAssignment
response_nonce:

```

1. 其中`version_info`取的是上一次成功收到的response中携带的`version_info`的值，如果是第一个请求，那么这个值就是空的，每一个type url的`version_info`是独立的。
2. `node`则是Envoy节点的相关信息
3. `resource_names` Envoy中每一类资源都是有名字的，通过携带这个字段可以告诉管理服务器只返回对应的资源即可，如果不携带则表示需要获取这类资源的全部
4. `type_url` 唯一标识一类资源的，ADS需要这个字段来识别出这个请求是哪种类型的
5. `response_nonce`

Reference:
1. https://github.com/envoyproxy/envoy/blob/master/api/XDS_PROTOCOL.md
2. https://developers.google.com/protocol-buffers/docs/proto3#any
3.


## Envoy生命周期callback

https://github.com/envoyproxy/envoy/pull/6254