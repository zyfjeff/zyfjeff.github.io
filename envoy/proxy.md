# 线程模型

单进程多线程架构，一个主线程控制各种零星的协调任务，多个worker线程执行listening、filtering、和数据转发等，只要一个连接被listener接受，那么这个连接上
所有的工作都是在一个线程中完成的。


# 热重启

1. 请求关闭Admin功能，并开始接管，这个过程是非原子的。

```cpp

  restarter_.shutdownParentAdmin(info);
  original_start_time_ = info.original_start_time_;
  admin_ = std::make_unique<AdminImpl>(initial_config.admin().profilePath(), *this);
  if (initial_config.admin().address()) {
    if (initial_config.admin().accessLogPath().empty()) {
      throw EnvoyException("An admin access log path is required for a listening server.");
    }
    ENVOY_LOG(info, "admin address: {}", initial_config.admin().address()->asString());
    admin_->startHttpListener(initial_config.admin().accessLogPath(), options.adminAddressPath(),
                              initial_config.admin().address(),
                              stats_store_.createScope("listener.admin."));
  } else {
    ENVOY_LOG(warn, "No admin address given, so no admin HTTP server started.");
  }

```

2. 创建ListenerManager、ClusterManagerFactory，一些管理数据结构

```cpp
overload_manager_ = std::make_unique<OverloadManagerImpl>(dispatcher(), stats(), threadLocal(),
                                                          bootstrap_.overload_manager());

// Workers get created first so they register for thread local updates.
listener_manager_ = std::make_unique<ListenerManagerImpl>(*this, listener_component_factory_,
                                                          worker_factory_, time_system_);

// The main thread is also registered for thread local updates so that code that does not care
// whether it runs on the main thread or on workers can still use TLS.
thread_local_.registerThread(*dispatcher_, true);

// We can now initialize stats for threading.
stats_store_.initializeThreading(*dispatcher_, thread_local_);

// Runtime gets initialized before the main configuration since during main configuration
// load things may grab a reference to the loader for later use.
runtime_loader_ = component_factory.createRuntime(*this, initial_config);

// Once we have runtime we can initialize the SSL context manager.
ssl_context_manager_ = std::make_unique<Ssl::ContextManagerImpl>(time_system_);

cluster_manager_factory_ = std::make_unique<Upstream::ProdClusterManagerFactory>(
    runtime(), stats(), threadLocal(), random(), dnsResolver(), sslContextManager(), dispatcher(),
    localInfo(), secretManager());

```

3. Bootstrap初始化

```cpp
  // Now the configuration gets parsed. The configuration may start setting thread local data
  // per above. See MainImpl::initialize() for why we do this pointer dance.
  Configuration::MainImpl* main_config = new Configuration::MainImpl();
  config_.reset(main_config);
  main_config->initialize(bootstrap_, *this, *cluster_manager_factory_);
```

  1. 初始化静态secret
  2. 初始化静态cluster
  3. 初始化静态listener
  4. 初始化ralimit service
  5. 初始化tracing
  6. 初始化sats sinks

3. 创建LDS API，注册init target

```cpp
  // Instruct the listener manager to create the LDS provider if needed. This must be done later
  // because various items do not yet exist when the listener manager is created.
  if (bootstrap_.dynamic_resources().has_lds_config()) {
    listener_manager_->createLdsApi(bootstrap_.dynamic_resources().lds_config());
  }
```


4. 集群初始化

```cpp
  // We need the RunHelper to be available to call from InstanceImpl::shutdown() below, so
  // we save it as a member variable.
  run_helper_ = std::make_unique<RunHelper>(*this, *dispatcher_, clusterManager(),
                                            access_log_manager_, init_manager_, overloadManager(),
                                            [this]() -> void { startWorkers(); });

  // starts.
  cm.setInitializedCb([&instance, &init_manager, &cm, workers_start_cb]() {
    if (instance.isShutdown()) {
      return;
    }

    // Pause RDS to ensure that we don't send any requests until we've
    // subscribed to all the RDS resources. The subscriptions happen in the init callbacks,
    // so we pause RDS until we've completed all the callbacks.
    cm.adsMux().pause(Config::TypeUrl::get().RouteConfiguration);

    ENVOY_LOG(info, "all clusters initialized. initializing init manager");

    // Note: the lamda below should not capture "this" since the RunHelper object may
    // have been destructed by the time it gets executed.
    init_manager.initialize([&instance, workers_start_cb]() {
      if (instance.isShutdown()) {
        return;
      }

      workers_start_cb();
    });
```

5. 等集群初始化完成，执行init manager

```cpp
    // Note: the lamda below should not capture "this" since the RunHelper object may
    // have been destructed by the time it gets executed.
    init_manager.initialize([&instance, workers_start_cb]() {
      if (instance.isShutdown()) {
        return;
      }

      workers_start_cb();
    });
```

会等到LDS、RDS、SDS的第一个配置拿到这个初始化才会完成，然后调用workers_start_cb

6. 开始worker，开启接收流量，并通知老进程drain listener

```cpp
void InstanceImpl::startWorkers() {
  listener_manager_->startWorkers(*guard_dog_);

  // At this point we are ready to take traffic and all listening ports are up. Notify our parent
  // if applicable that they can stop listening and drain.
  restarter_.drainParentListeners();
  drain_manager_->startParentShutdownSequence();
}
```

* 不主动过关闭连接，连接的关闭依靠空闲时间、被动关闭还有shutdown(依赖--parent-shutdown-time-s)


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
5. `response_nonce` 用于显示的对`DiscoverResponse`做ack

> 如果请求被接收了，那么envoy会进行ack，返回的response_nonce对应DiscoveryResponse中的nonce，version_info则对应DiscoveryResponse中的version_info
> 如果拒绝了DiscoveryResponse则返回的response_nonce对应DiscoveryResponse中的nonce，version_info则对应上一次DiscoveryResponse中的version_info

LDS/CDS的resource_names一般为空，表示获取所有的cluster和listener资源，而EDS和RDS一般会带上resource name获取感兴趣的资源，这个resource name来自于LDS和CDS。
如果一个EDS没有对应的CDS，那么这个EDS是无效的，Envoy会忽略这个EDS。

LDS和CDS请求存在requesting wramming的过程，要求获取到LDS和EDS以及依赖的EDS和RDS，Envoy才算初始化完成。

xDS对于各个资源的顺序没有约束，因为每一个资源都是一个单独的流，资源的到达顺序会导致流量存在下降的问题，比如，LDS就绪了但是依赖的CDS还没有继续，那么这会导致请求无效。
但是如果CDS先就绪，然后LDS再就绪就可以避免这个问题了。所以为了避免因为资源更新的顺序问题导致流量被drop，要求资源更新的顺序如下:

* CDS updates (if any) must always be pushed first.
* EDS updates (if any) must arrive after CDS updates for the respective clusters.
* LDS updates must arrive after corresponding CDS/EDS updates.
* RDS updates related to the newly added listeners must arrive after CDS/EDS/LDS updates.
* VHDS updates (if any) related to the newly added RouteConfigurations must arrive after RDS updates.
* Stale CDS clusters and related EDS endpoints (ones no longer being referenced) can then be removed.

为了保证资源请求的顺序可以按照上述定义的顺序，需要将所有的资源请求和响应控制在同一个流中。

xDS中资源的更新是没办法单独推送的，每次推送的资源都是全量的，即使其中部分资源发生了变更，都是全量下发，envoy则会进行资源对比，有变更的则进行应用。
这样带来的传输消耗还是蛮大的，为此有了`Incremental xDS`，只下发有变更的资源。

`Incremental xDS` 一般用在以下几个场景:

* Initial message in a xDS bidirectional gRPC stream.

* As an ACK or NACK response to a previous DeltaDiscoveryResponse.
In this case the response_nonce is set to the nonce value in the Response. ACK or NACK is determined by the absence or presence of error_detail.

* Spontaneous DeltaDiscoveryRequest from the client.
This can be done to dynamically add or remove elements from the tracked resource_names set. In this case response_nonce must be omitted.

Reference:
1. https://github.com/envoyproxy/envoy/blob/master/api/XDS_PROTOCOL.md
2. https://developers.google.com/protocol-buffers/docs/proto3#any


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

4. Resource 每一个资源Monitor和一个Resource关联起来，这个Resource提供了

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

## Envoy internal header


## Cluster Manager

Cluster warming，当server启动的时候或者通过CDS初始化Cluster时，Cluster是处于warmed状态的，这意味着
此时集群并不可用，直到下面两个事情完成:

1. 初始服务发现负载（例如，DNS解析，EDS更新等）.
2. 完成第一次健康检查

对于新增集群处于warmed状态来说，如果HTTP router路由到这个集群，此时会返回404/503(依据配置来)
对于更新已有集群的情况来说，在更新的集群还没有初始化完成之前，使用老的集群来分发流量

ClusterManager 管理连接池和load balancing，在多个work线程共享这个对象


## Load balancer

一个集群下面可以配置多个Priority和多个Locality，每一个Priority和Locality可以包含一组endpoint，Priority和Locality是多对多的关系
也就是说一个Priority可以包含多个Locality的机器列表，一个Locality也可以包含多个priority，当一个priority中的主机不可用/不健康的时候
会去从下一个priority中进行选择。

PrioritySetImpl 包含了一个集群下的所有host，然后按照priority维度对给定集群进行分组，一个集群会有多个Priority
HostSetImpl 按照priority来维护一组host集合，构造函数需要提供priority，然后通过updateHosts来添加host。这组host可能来自于多个Locality
HostDescriptionImpl 对upstream主机的一个描述接口，比如是否是canary、metadata、cluster、healthChecjer、hostname、address等
HostImpl 代表一个upstream Host
HostPerLocalityImpl 按照locality的维度组织host集合，核心数据成员`std::vector<HostVector>`，第一个host集合是local locality。
PriorityStateManager 管理一个集群的PriorityState，而PriorityState则是按照priority分组维护的一组host和对应的locality weight map


PrioritySetImpl包含了多个HostSetImpl，一个HostSetImpl则包含了一个HostPerLocalityImpl。


```yaml
ClusterLoadAssignment.Policy
{
  "drop_overloads": [],
  "overprovisioning_factor": "{...}",
  "endpoint_stale_after": "{...}"
}
```
overprovisioning_factor一个过度配置的因子，默认是140，当一个priority或者一个locality中的健康主机百分比乘以这个因子小于100，那么就认为这个priority或者locality是不健康的。


## Degraded endpoints

Envoy支持将某些endpoint标记为degraded的，这种类型的endpoint可以接收流量，但是只有在没有足够的健康机器的时候才会接收流量来处理
上游主机通过在返回的header中添加`x-envoy-degraded`头来表明当前的健康检查的主机是degraded。

## Priority levels

当某一个Priority是健康的，那么会接收全部流量，但是如果发现流这个Priority是不健康的，那么就需要按照100%比将流量打到下一个优先级。
比如健康程度是71%，那么0.71 * 140约等于99%，那么这个Priorit就接收99%的流量，剩下的1%流量就发送到下一个Priority。如果所有的Priority加起来的健康程度不满足100%，还需要
将流量按比例缩放。比如有两个Priority，第一个Priority的健康程度是20%，第二个是30%，总的健康程度只有50%，那么流量就会往第一个Priority打40，第二个Priority打60%，而不是
0.2 * 140的流量打到第一个Priority了。

## Panic threshold

正常情况下Envoy做负载均衡的时候，会考虑到healthy和degraded的机器所占的比例。但是如果这个比例太低的情况下，Envoy则不考虑健康状况会在所有的机器上进行balancer，而这个被称为panic，
这个比例是panic阀值，默认是50%。如果host的健康程度低于72%就会将流量打向低一级的Priority，如果所有的Priority的健康程序程度比例加起来少于100%，那么哪个优先级低于50%，那么这个优先级
就处于panic模式，流量是发到这个Priority下所有的机器。如果所有的Priority的健康程序程度比例加起来等于或大于100%就都不会出现panic模式，即使某些Priority的健康程度是小于50%的。

## Zone aware routing

1. 要求始发集群和upstream集群不是panic mode
2. Zone aware routing是开启的(配置中的`routing_enabled`字段表示进行zone aware的请求百分百，默认是100%)
3. 始发集群和upstream集群有相同的zone数量
4. upstream集群有足够的机器(`min_cluster_size` zone aware集群的最小大小)
5. 如果始发集群中的本地zone占比要比upstream集群中的本地zone占比高的话，那么Envoy会按照本地zone所在占比例只转发部分流量到upstream集群中，剩余的流量进行跨区域转发
6. 如果始发集群中的本地zone占比要比upstream集群中的本地zone占比小的话，所有的流量都会直接打到upstream中的本地zone

需要定一个一个本地集群，这个集群就是Envoy网格组成的集群

```yaml
cluster_manager:
  local_cluster_name: "local_cluster_name"
```

```yaml
{
  "routing_enabled": "{...}",
  "min_cluster_size": "{...}"
}
```

## Load Ststs reporter

```yaml
config.bootstrap.v2.ClusterManager:
{
  "local_cluster_name": "...",
  "outlier_detection": "{...}",
  "upstream_bind_config": "{...}",
  "load_stats_config": "{...}"
}
```

load_stats_config 用于配置管理集群，启动的时候，会等待管理平面发送LoadStatsResponse，响应中带有需要获取stats的集群名词列表，Envoy会定时将这些集群的stats信息发送到控制平面。

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

## Cluster subset

本质上是将一个集群拆分成多个子集群，Envoy中称为subset，而subset的划分是根据每一个endpoint中携带的metadata来组织subset集合。

```yaml
{
  "fallback_policy": "...",
  "default_subset": "{...}",
  "subset_selectors": [],
  "locality_weight_aware": "...",
  "scale_locality_weight": "...",
  "panic_mode_any": "..."
}
```

* fallback_policy:

  1. NO_ENDPOINT 如果没有匹配的subset就失败
  2. ANY_ENDPOINT 如果没有匹配的subset就从整个集群中按照负载均衡的策略来选择
  3. DEFAULT_SUBSET 匹配指定的默认subset，如果机器都没有指定subset那么就使用ANY_ENDPOINT

* default_subset

指定一个set集合，用来在fallback的时候，进行选择

* locality_weight_aware


* subset_selectors

用来表明subset如何生成，下面这个例子就是表明带有a和b的metadata的机器会组成一个subset。如果一个机器带有a、b、x三个metadata，
那么这台机器就会属于两个subset。

```json
{
  "subset_selectors": [
    { "keys": [ "a", "b" ] },
    { "keys": [ "x" ] }
  ]
}
```
* locality_weight_aware

* scale_locality_weight

* panic_mode_any

默认子集不为空，但是通过fallback策略使用默认子集的时候，仍然没有选择到主机，那么通过这个选项会使得从所有机器中选择一台机器来进行路由


* Example

Endpoint | stage | version | type   | xlarge
---------|-------|---------|--------|-------
e1       | prod  | 1.0     | std    | true
e2       | prod  | 1.0     | std    |
e3       | prod  | 1.1     | std    |
e4       | prod  | 1.1     | std    |
e5       | prod  | 1.0     | bigmem |
e6       | prod  | 1.1     | bigmem |
e7       | dev   | 1.2-pre | std    |

Note: Only e1 has the "xlarge" metadata key.

Given this CDS `envoy::api::v2::Cluster`:

``` json
{
  "name": "c1",
  "lb_policy": "ROUND_ROBIN",
  "lb_subset_config": {
    "fallback_policy": "DEFAULT_SUBSET",
    "default_subset": {
      "stage": "prod",
      "version": "1.0",
      "type": "std"
    },
    "subset_selectors": [
      { "keys": [ "stage", "type" ] },
      { "keys": [ "stage", "version" ] },
      { "keys": [ "version" ] },
      { "keys": [ "xlarge", "version" ] },
    ]
  }
}
```

下面这些subset将会生成:

stage=prod, type=std (e1, e2, e3, e4)
stage=prod, type=bigmem (e5, e6)
stage=dev, type=std (e7)
stage=prod, version=1.0 (e1, e2, e5)
stage=prod, version=1.1 (e3, e4, e6)
stage=dev, version=1.2-pre (e7)
version=1.0 (e1, e2, e5)
version=1.1 (e3, e4, e6)
version=1.2-pre (e7)
version=1.0, xlarge=true (e1)

默认的subset
stage=prod, type=std, version=1.0 (e1, e2)


Example:

```yaml
    filter_chains:
    - filters:
      - name: envoy.http_connection_manager
        config:
          codec_type: auto
          stat_prefix: ingress_http
          route_config:
            name: local_route
            virtual_hosts:
            - name: backend
              domains:
              - "*"
              routes:
              - match:
                  prefix: "/"
                route:
                  cluster: service1
                  metadata_match:
                    filter_metadata:
                      envoy.lb:
                        env: taobao
          http_filters:
          - name: envoy.router
            config: {}
  clusters:
  - name: service1
    connect_timeout: 0.25s
    type: strict_dns
    lb_policy: round_robin
    lb_subset_config:
      fallback_policy: DEFAULT_SUBSET
      default_subset:
        env: "taobao"
      subset_selectors:
      - keys:
        - env
    load_assignment:
      cluster_name: service1
      endpoints:
        lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: 8081
          metadata:
            filter_metadata:
              envoy.lb:
                env: hema

        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: 8082
          metadata:
            filter_metadata:
              envoy.lb:
                env: taobao
```

Reference: https://github.com/envoyproxy/envoy/blob/master/source/docs/subset_load_balancer.md

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

## 命令行参数

* `--allow-unknown-fields` 关闭protobuf验证，忽略新增字段，核心实现就是 `message.GetReflection()->GetUnknownFields`

```cpp

  allow_unknown_fields_ = allow_unknown_fields.getValue();
  if (allow_unknown_fields_) {
    MessageUtil::proto_unknown_fields = ProtoUnknownFieldsMode::Allow;
  }

  // 核心实现
  static void checkUnknownFields(const Protobuf::Message& message) {
    if (MessageUtil::proto_unknown_fields == ProtoUnknownFieldsMode::Strict &&
        !message.GetReflection()->GetUnknownFields(message).empty()) {
      throw EnvoyException("Protobuf message (type " + message.GetTypeName() +
                            ") has unknown fields");
    }
  }
```

* `--enable-mutex-tracing` 主要是利用了absl库提供的`RegisterMutexTracer`，将tracer信息通过callback的方式暴露出来

```cpp
MutexTracerImpl& MutexTracerImpl::getOrCreateTracer() {
  if (singleton_ == nullptr) {
    singleton_ = new MutexTracerImpl;
    // There's no easy way to unregister a hook. Luckily, this hook is innocuous enough that it
    // seems safe to leave it registered during testing, even though this technically breaks
    // hermeticity.
    absl::RegisterMutexTracer(&Envoy::MutexTracerImpl::contentionHook);
  }
  return *singleton_;
}

```

* `--drain-time-s`

```cpp

  // 连接中有流量的时候才会实际走到这段代码，然后进行Drain处理
  void ConnectionManagerImpl::ActiveStream::encodeHeaders(ActiveStreamEncoderFilter* filter,
                                                        HeaderMap& headers, bool end_stream) {
    .....
    // See if we want to drain/close the connection. Send the go away frame prior to encoding the
    // header block.
    if (connection_manager_.drain_state_ == DrainState::NotDraining &&
        connection_manager_.drain_close_.drainClose()) {

      // This doesn't really do anything for HTTP/1.1 other then give the connection another boost
      // of time to race with incoming requests. It mainly just keeps the logic the same between
      // HTTP/1.1 and HTTP/2.
      connection_manager_.startDrainSequence();
      connection_manager_.stats_.named_.downstream_cx_drain_close_.inc();
      ENVOY_STREAM_LOG(debug, "drain closing connection", *this);
    }
    .......
   }

  void ConnectionManagerImpl::startDrainSequence() {
    ASSERT(drain_state_ == DrainState::NotDraining);
    drain_state_ = DrainState::Draining;
    codec_->shutdownNotice();
    drain_timer_ = read_callbacks_->connection().dispatcher().createTimer(
        [this]() -> void { onDrainTimeout(); });
    drain_timer_->enableTimer(config_.drainTimeout());
  }

  // Drain 超时时间，默认是5000ms，如果是http2的协议会先发一个goAway协议帧，如果是http协议就什么都不做
  void ConnectionManagerImpl::onDrainTimeout() {
    ASSERT(drain_state_ != DrainState::NotDraining);
    codec_->goAway();
    drain_state_ = DrainState::Closing;
    checkForDeferredClose();
  }

  // 最后会调用close
  void ConnectionManagerImpl::checkForDeferredClose() {
    if (drain_state_ == DrainState::Closing && streams_.empty() && !codec_->wantsToWrite()) {
      read_callbacks_->connection().close(Network::ConnectionCloseType::FlushWriteAndDelay);
    }
  }

```

* `--file-flush-interval-msec` access log文件的刷盘间隔


## Drain manager的实现


1. 基础接口类

```cpp


// 网络层用来决策什么时候执行drain
class DrainDecision {
public:
  virtual ~DrainDecision() {}
  // 用于判断一个连接是否要closed并且进行drain处理
  virtual bool drainClose() const PURE;
};

class DrainManager : public Network::DrainDecision {
public:
  virtual void startDrainSequence(std::function<void()> completion) PURE;
  virtual void startParentShutdownSequence() PURE;
};
```

2. DrainManagerImpl 实现

```cpp
class DrainManagerImpl : Logger::Loggable<Logger::Id::main>, public DrainManager {
public:
  DrainManagerImpl(Instance& server, envoy::api::v2::Listener::DrainType drain_type);

  // Server::DrainManager
  bool drainClose() const override;
  void startDrainSequence(std::function<void()> completion) override;
  void startParentShutdownSequence() override;

private:
  bool draining() const { return drain_tick_timer_ != nullptr; }
  void drainSequenceTick();

  Instance& server_;
  const envoy::api::v2::Listener::DrainType drain_type_;
  Event::TimerPtr drain_tick_timer_;
  std::atomic<uint32_t> drain_time_completed_{};
  Event::TimerPtr parent_shutdown_timer_;
  std::function<void()> drain_sequence_completion_;
};

// 就是一个timer，指定的时间到达后就开始执行drain callback
void DrainManagerImpl::startDrainSequence(std::function<void()> completion) {
  drain_sequence_completion_ = completion;
  ASSERT(!drain_tick_timer_);
  drain_tick_timer_ = server_.dispatcher().createTimer([this]() -> void { drainSequenceTick(); });
  drainSequenceTick();
}

// 不停的计数和打印
void DrainManagerImpl::drainSequenceTick() {
  ENVOY_LOG(trace, "drain tick #{}", drain_time_completed_.load());
  ASSERT(drain_time_completed_.load() < server_.options().drainTime().count());
  ++drain_time_completed_;

  if (drain_time_completed_.load() < server_.options().drainTime().count()) {
    drain_tick_timer_->enableTimer(std::chrono::milliseconds(1000));
  } else if (drain_sequence_completion_) {
    drain_sequence_completion_();
  }
}

// 在hotrestart的时候，子进程通知父进程closing listener然后开始执行drain
void InstanceImpl::drainListeners() {
  ENVOY_LOG(info, "closing and draining listeners");
  listener_manager_->stopListeners();
  drain_manager_->startDrainSequence(nullptr);
}
```

ListenerImpl实现了Network::DrainDecision接口

```cpp
bool ListenerImpl::drainClose() const {
  // When a listener is draining, the "drain close" decision is the union of the per-listener drain
  // manager and the server wide drain manager. This allows individual listeners to be drained and
  // removed independently of a server-wide drain event (e.g., /healthcheck/fail or hot restart).
  return local_drain_manager_->drainClose() || parent_.server_.drainManager().drainClose();
}
```


http的conn manager每次在处理请求的时候会根据listenerImpl的drianClosed的结果来决定是否进行drain处理

```cpp
  // See if we want to drain/close the connection. Send the go away frame prior to encoding the
  // header block.
  if (connection_manager_.drain_state_ == DrainState::NotDraining &&
      connection_manager_.drain_close_.drainClose()) {

    // This doesn't really do anything for HTTP/1.1 other then give the connection another boost
    // of time to race with incoming requests. It mainly just keeps the logic the same between
    // HTTP/1.1 and HTTP/2.
    connection_manager_.startDrainSequence();
    connection_manager_.stats_.named_.downstream_cx_drain_close_.inc();
    ENVOY_STREAM_LOG(debug, "drain closing connection", *this);
  }
```

如果要进行drain处理就开始执行Drain

```cpp
void ConnectionManagerImpl::startDrainSequence() {
  ASSERT(drain_state_ == DrainState::NotDraining);
  drain_state_ = DrainState::Draining;
  // http1的话什么都不做，http2则会发送GOAWAY进行优雅关闭
  codec_->shutdownNotice();
  drain_timer_ = read_callbacks_->connection().dispatcher().createTimer(
      [this]() -> void { onDrainTimeout(); });
  drain_timer_->enableTimer(config_.drainTimeout());
}

void ConnectionManagerImpl::onDrainTimeout() {
  ASSERT(drain_state_ != DrainState::NotDraining);
  codec_->goAway();
  drain_state_ = DrainState::Closing;
  checkForDeferredClose();
}
```