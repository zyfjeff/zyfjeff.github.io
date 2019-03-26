# 线程模型

单进程多线程架构，一个主线程控制各种零星的协调任务，多个worker线程执行listening、filtering、和数据转发等，只要一个连接被listener接受，那么这个连接上
所有的工作都是在一个线程中完成的。

# 性能优化

1. `--disable-hot-restart` 关闭热重启，避免使用共享内存存放stats统计信息
2. `--concurrency` 和绑定的核心数一致，避免多个CPU切换导致cache失效
3. 关闭stats统计和添加自定义header

```yaml
http_filters:
- name: envoy.router
  config:
      dynamic_stats: false
      suppress_envoy_headers: false
```

4. 关闭circuit_breakers

```yaml
circuit_breakers:
  thresholds:
  - priority: HIGH
    max_connections: 1000000000
    max_pending_requests: 1000000000
    max_requests: 1000000000
    max_retries: 1000000000
```

# listener

```json
{
  "name": "...",
  "address": "{...}",
  // listener filter
  "filter_chains": [],
  // 废弃了，改用original dst filter
  "use_original_dst": "{...}",
  // 设置writer buffer的大小限制，这个指的是envoy忘downstream、或者往upstream写入数据的buffer大小
  "per_connection_buffer_limit_bytes": "{...}",
  "metadata": "{...}",
  // listener何时析构，默认在listener的移除和修改、还有热重启、/healthcheck/fail的时候
  // 也可以设置为仅仅在listener的移除和修改、还有热重启的时候析构
  "drain_type": "...",
  // listener filter
  "listener_filters": [],
  "listener_filters_timeout": "{...}",
  //
  "transparent": "{...}",
  // 可以监听在一个非本地ip上，通常配置original dst filter来做listener的匹配
  // 本质上就是给socket设置IP_FREEBIND option参数
  "freebind": "{...}",
  "socket_options": [],
  // 第一次三次握手生成cookie后，第二次直接带着cookie就避免了三次握手。
  // 这个参数用来设置FAST OPEN的队列长度。
  // 就是设置socket的TCP_FASTOPEN option参数
  "tcp_fast_open_queue_length": "{...}"
}
```

* `per_connection_buffer_limit_bytes`

```cpp
void ConnectionHandlerImpl::ActiveListener::newConnection(Network::ConnectionSocketPtr&& socket) {
  // Find matching filter chain.
  const auto filter_chain = config_.filterChainManager().findFilterChain(*socket);
  if (filter_chain == nullptr) {
    ENVOY_LOG_TO_LOGGER(parent_.logger_, debug,
                        "closing connection: no matching filter chain found");
    stats_.no_filter_chain_match_.inc();
    socket->close();
    return;
  }

  auto transport_socket = filter_chain->transportSocketFactory().createTransportSocket(nullptr);
  // 创建连接后，设置buffer limit
  Network::ConnectionPtr new_connection =
      parent_.dispatcher_.createServerConnection(std::move(socket), std::move(transport_socket));
  new_connection->setBufferLimits(config_.perConnectionBufferLimitBytes());

  const bool empty_filter_chain = !config_.filterChainFactory().createNetworkFilterChain(
      *new_connection, filter_chain->networkFilterFactories());
  if (empty_filter_chain) {
    ENVOY_CONN_LOG_TO_LOGGER(parent_.logger_, debug, "closing connection: no filters",
                             *new_connection);
    new_connection->close(Network::ConnectionCloseType::NoFlush);
    return;
  }

  onNewConnection(std::move(new_connection));
}

void ConnectionImpl::setBufferLimits(uint32_t limit) {
  read_buffer_limit_ = limit;
  // 其实就是注册了一个writer buffer的水位回调
  if (limit > 0) {
    static_cast<Buffer::WatermarkBuffer*>(write_buffer_.get())->setWatermarks(limit + 1);
  }
}
```


Reference: http://man7.org/linux/man-pages/man7/ip.7.html


# listener filter
作用在listen socket上，当有连接到来的时候，通过libevent会触发可读事件，调用listen socket的accept获取到连接socket封装为ConnectionSocket，
最后调用ActiveListener::onAccept，将获取到的连接socket作为其参数。

1. 创建filter chain
2. continueFilterChain 调用filter chain
3. 如果有filter返回了StopIteration，那么就开启timer，在这个时间内还没有继续continue就直接关闭当前socket
4. filter返回StopIteration后，要继续运行剩下的filter可以回调continueFilterChain

> 比如proxy_protocol这个listener filter当接收到一个filter后会注册读事件，从socket读取proxy协议头，所以会返回StopIteration
> 等到有数据可读的时候，并且读到了协议头才会回调continueFilterChain继续执行下面的filter

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
    // 启动了一个timer，避免filter长时间不调用
    active_socket->startTimer();
    active_socket->moveIntoListBack(std::move(active_socket), sockets_);
  }
}

// 如果超时就从socket list中移除当前socket
void ConnectionHandlerImpl::ActiveSocket::onTimeout() {
  listener_.stats_.downstream_pre_cx_timeout_.inc();
  ASSERT(inserted());
  unlink();
}

void ConnectionHandlerImpl::ActiveSocket::startTimer() {
  if (listener_.listener_filters_timeout_.count() > 0) {
    timer_ = listener_.parent_.dispatcher_.createTimer([this]() -> void { onTimeout(); });
    timer_->enableTimer(listener_.listener_filters_timeout_);
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
TODO(tianqian.zyf): 例子

## original_src filter

L3/L4 transparency的含义: L3要求源IP可见、L4要求端口可见，这个filter的目的是将原地址信息透传到upstream，让upstream可以
获取到真实的源IP和端口信息。

TODO(tianqian.zyf): 例子

Reference: https://github.com/envoyproxy/envoy/pull/5255
Reference: https://docs.google.com/document/d/1md08XUBfVG9FwPUZixhR3f77dFRCVGJz2359plhzXxo/edit


## proxy_protocol filter
建立连接后发送一段数据来传递源地址和端口信息。

```cpp
// 连接建立后，开始注册读事件，读取传递过来的数据。
Network::FilterStatus Filter::onAccept(Network::ListenerFilterCallbacks& cb) {
  ENVOY_LOG(debug, "proxy_protocol: New connection accepted");
  Network::ConnectionSocket& socket = cb.socket();
  ASSERT(file_event_.get() == nullptr);
  file_event_ =
      cb.dispatcher().createFileEvent(socket.ioHandle().fd(),
                                      [this](uint32_t events) {
                                        ASSERT(events == Event::FileReadyType::Read);
                                        onRead();
                                      },
                                      Event::FileTriggerType::Edge, Event::FileReadyType::Read);
  cb_ = &cb;
  return Network::FilterStatus::StopIteration;
}

void Filter::onRead() {
  try {
    onReadWorker();
  } catch (const EnvoyException& ee) {
    config_->stats_.downstream_cx_proxy_proto_error_.inc();
    cb_->continueFilterChain(false);
  }
}

// 读取proxy头，这里的读取是通过ioctl(fd, FIONREAD, &bytes_avail) 来获取缓存中的数据大小 ,
// 然后通过MSG_PEEK的方式查看数据。并不是直接read，因为一旦不是proxy ptorocol协议头会导致数据不完整(被读走了)。

void Filter::onReadWorker() {
  Network::ConnectionSocket& socket = cb_->socket();

  if ((!proxy_protocol_header_.has_value() && !readProxyHeader(socket.ioHandle().fd())) ||
      (proxy_protocol_header_.has_value() && !parseExtensions(socket.ioHandle().fd()))) {
    // We return if a) we do not yet have the header, or b) we have the header but not yet all
    // the extension data. In both cases we'll be called again when the socket is ready to read
    // and pick up where we left off.
    return;
  }
  ....
  // 读取完成后，拿到获取的源地址信息进行操作。

  // Only set the local address if it really changed, and mark it as address being restored.
  if (*proxy_protocol_header_.value().local_address_ != *socket.localAddress()) {
    socket.restoreLocalAddress(proxy_protocol_header_.value().local_address_);
  }
  socket.setRemoteAddress(proxy_protocol_header_.value().remote_address_);
  ....
}
```
TODO(tianqian.zyf): 例子
Reference: https://www.haproxy.org/download/1.8/doc/proxy-protocol.txt

## TLS Inspector filter

TLS Inspector listener filter allows detecting whether the transport appears to be TLS or plaintext, and if it is TLS, it detects the Server Name Indication and/or Application-Layer Protocol Negotiation from the client. This can be used to select a FilterChain via the server_names and/or application_protocols of a FilterChainMatch.


1. 注册读数据，等待数据到来
2. 解析Client hello报文
3. 找到TLS信息，设置TransportSocket为Tls


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


## Envoy四层filter执行链

1. Downstream连接建立后，开始创建filter，然后初始化filter
2. 回调onNewConnection
3. 回调onData

```cpp
bool FilterManagerImpl::initializeReadFilters() {
  if (upstream_filters_.empty()) {
    return false;
  }
  // 初始化完成后，开始从头开始执行filter
  onContinueReading(nullptr);
  return true;
}

// 传入的是nullptr的时候，从头开始执行filter的
// 设置initialized_标志为true
// 回调onNewConnection，如果是返回stop就停止运行了
// 等待filter返回通过ReadFilterCallbacks回调onContinueReading来继续执行
void FilterManagerImpl::onContinueReading(ActiveReadFilter* filter) {
  std::list<ActiveReadFilterPtr>::iterator entry;
  if (!filter) {
    entry = upstream_filters_.begin();
  } else {
    entry = std::next(filter->entry());
  }

  for (; entry != upstream_filters_.end(); entry++) {
    if (!(*entry)->initialized_) {
      (*entry)->initialized_ = true;
      FilterStatus status = (*entry)->filter_->onNewConnection();
      if (status == FilterStatus::StopIteration) {
        return;
      }
    }

    BufferSource::StreamBuffer read_buffer = buffer_source_.getReadBuffer();
    if (read_buffer.buffer.length() > 0 || read_buffer.end_stream) {
      FilterStatus status = (*entry)->filter_->onData(read_buffer.buffer, read_buffer.end_stream);
      if (status == FilterStatus::StopIteration) {
        return;
      }
    }
  }
}
```

Example:
有三个filter、其中任何一个filter其中的一个callback返回StopIteration那么整个流程就停止了，需要等待调用onContinueReading才能继续
执行下一个callback方法。

FilterA::onNewConnection
FilterA::onData

FilterB::onNewConnection
FilterB::onData

FilterC::onNewConnection
FilterC::onData

执行顺序为:
FilterA::onNewConnection->FilterA::onData->FilterB::onNewConnection->FilterB::onData->FilterC::onNewConnection->FilterC::onData
任何一个callback返回StopIteration整个流程就不会继续往下走了，需要等待对应的filter回调onContinueReading，这样就会带来一个问题，一旦停止filter chain
继续往下走，那么网络层依然会收数据存在内部buffer里面，这会导致内存上涨，因此TCP PROXY中会在调用onNewConnection的时候先关闭读，然后和upstream建立连接
连接建立后才会开启读，防止内存被打爆。


## Overload manager

核心概念:

1. ResourceMonitors 内置了一系列要监控的资源，目前有的是FixedHeap

2. OverloadAction 触发资源的限制的后，可以进行的一系列action，提供了isActive来表明当前Action是否有效(有Trigger就是有效的)

  * envoy.overload_actions.stop_accepting_requests
  * envoy.overload_actions.disable_http_keepalive
  * envoy.overload_actions.stop_accepting_connections
  * envoy.overload_actions.shrink_heap

3. Trigger 每一个OverloadAction会包含一个触发器，里面设置了阀值，提供了isFired来表明当前触发器是否触发了

Setp1: 实现核心接口

```cpp
class ResourceMonitor {
public:
  virtual ~ResourceMonitor() {}
  class Callbacks {
  public:
    virtual ~Callbacks() {}
    virtual void onSuccess(const ResourceUsage& usage) = 0;
    virtual void onFailure(const EnvoyException& error) = 0;
  };
  virtual void updateResourceUsage(Callbacks& callbacks) = 0;
```

每一类资源需要去实现这个接口，例如:FixedHeap

```cpp
class FixedHeapMonitor : public Server::ResourceMonitor {
public:
  FixedHeapMonitor(
      const envoy::config::resource_monitor::fixed_heap::v2alpha::FixedHeapConfig& config,
      std::unique_ptr<MemoryStatsReader> stats = std::make_unique<MemoryStatsReader>());

  void updateResourceUsage(Server::ResourceMonitor::Callbacks& callbacks) override;

private:
  const uint64_t max_heap_;
  std::unique_ptr<MemoryStatsReader> stats_;
};
```

Setp2: 定期回调updateResourceUsage

OverloadManagerImpl初始化的时候，会对各种资源的Monitor进行构造、以及OverloadAction等
然后开始启动timer，定期回调updateResourceUsage接口，然后更新thread_local状态。

```cpp

void OverloadManagerImpl::start() {
  ASSERT(!started_);
  started_ = true;

  tls_->set([](Event::Dispatcher&) -> ThreadLocal::ThreadLocalObjectSharedPtr {
    return std::make_shared<ThreadLocalOverloadState>();
  });

  if (resources_.empty()) {
    return;
  }

  timer_ = dispatcher_.createTimer([this]() -> void {
    for (auto& resource : resources_) {
      resource.second.update();
    }

    timer_->enableTimer(refresh_interval_);
  });
  timer_->enableTimer(refresh_interval_);
}
```


## IP Transparency

将Downstream的remote address传递到upstream

1. HTTP Headers
通过x-forwarded-for头，仅支持HTTP协议

2. Proxy Protocol
TCP建立连接的时候，将源地址信息传递过来，需要upstream的主机支持

3. Original Source Listener Filter
Envoy通过socket可以拿到远程的地址也可以借助Proxy Protocol拿到downstream的地址信息、然后把地址信息设置到upstream的socket中，upstream在返回的时候
需要将其流量指向envoy，这里需要让Envoy和upstream部署在一起，然后使用iptables来完成。

TODO(tianqian.zyf): 例子

## istio-iptables.sh 分析

* `-p` 指定envoy的端口，默认是15001，指的是拦截到的流量将导入到哪个端口，默认是15001
* `-u` 指定UID，Effective UID等于指定的这个UID的程序流量不被拦截，默认是1337
* `-g` 指定GID，Effective GID等于指定的这个GID的程序流量不被拦截，默认是1337
* `-m` 指定流量拦截的模式，REDIRECT还是TPROXY
* `-b` 指定哪些inbound的端口被重定向到Envoy中，多个端口按照逗号分割，默认是所有的流量。
* `-d` 排除哪些inbound端口不进行流量的重定向
* `-i` 指定IP CIDR进行outbound的流量重定向
* `-x` 排查哪些IP不进行outbound的流量重定向
* `-k` 从哪个虚拟口出去的流量被认为是outbound


REDIRECT模式

```

```