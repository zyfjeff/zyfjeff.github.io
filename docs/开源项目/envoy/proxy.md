# Envoy源码分析


## 线程模型

单进程多线程架构，一个主线程控制各种零星的协调任务，多个worker线程执行listening、filtering、和数据转发等，只要一个连接被listener接受，那么这个连接上
所有的工作都是在一个线程中完成的。


## 热重启

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


## 性能优化

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


## listener filter
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

TLS Inspector listener filter allows detecting whether the transport appears to be TLS or plaintext, and if it is TLS,
it detects the Server Name Indication and/or Application-Layer Protocol Negotiation from the client.
This can be used to select a FilterChain via the server_names and/or application_protocols of a FilterChainMatch.


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
5. 显示的host重写
6. 自动根据选择的upstream的DNS name进行host重写
7. 前缀重写
8. 基于正则表达式捕获组的方式进行path重写
9. 请求重试(基于配置和http header两种方式)
10. 请求超时控制(基于配置和http header两种方式)
11. Request hedging
12. Traffic shifting
13. Traffic splitting
14. 任何的header match
15. 可以指定虚拟集群
16. 基于优先级的路由
17. 基于hash policy的路由
18. 基于绝对url的代理转发


* Route Scope

For example, for the following scoped route configuration, Envoy will look into the “addr” header value, split the header value by “;” first,
and use the first value for key ‘x-foo-key’ as the scope key. If the “addr” header value is “foo=1;x-foo-key=127.0.0.1;x-bar-key=1.1.1.1”, then “127.0.0.1”
will be computed as the scope key to look up for corresponding route configuration.

```
HttpConnectionManager config:
scoped_routes:
  name: foo-scoped-routes
  scope_key_builder:
    fragments:
      - header_value_extractor:
          name: X-Route-Selector
          element_separator: ,
          element:
            separator: =
            key: vip


scoped_route_configurations_list/SRDS:
 (1)
 name: route-scope1
 route_configuration_name: route-config1
 key:
    fragments:
      - string_key: 172.10.10.20

(2)
 name: route-scope2
 route_configuration_name: route-config2
 key:
   fragments:
     - string_key: 172.20.20.30

GET / HTTP/1.1
Host: foo.com
X-Route-Selector: vip=172.10.10.20

最终匹配到route-config1
```

根据`scoped_routes`获取请求中的指定字段，然后切割获取到对应的value，然后拿着value和定义的`scoped_route_configurations`知道要使用的路由配置名称


* 重试语义

```yaml
{
  "retry_on": "...",
  "num_retries": "{...}",
  "per_try_timeout": "{...}",
  "retry_priority": "{...}",
  "retry_host_predicate": [],
  "host_selection_retry_max_attempts": "...",
  "retriable_status_codes": [],
  "retry_back_off": "{...}",
  "retriable_headers": [],
  "retriable_request_headers": []
}
```

可以配置重试的最大次数、重试的条件(可以是基于reseponse code、可以是网络等等)，Request budgets可以防止大量的请求重试、可以进行host selection rerty



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
> 同一时间有多个DiscoveryRequest的时候，mangement server只会影响最后的一个DiscoverRequest
> 如果管理server返回的response_nonce是一个新的值，Envoy会拒绝这次请求


If Envoy had instead rejected configuration update X, it would reply with error_detail populated and its previous version, which in this case was the empty initial version. The error_detail has more details around the exact error message populated in the message field:

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


```yaml
DeltaDiscoveryRequest
{
  "node": "{...}",
  "type_url": "...",
  "resource_names_subscribe": [],
  "resource_names_unsubscribe": [],
  "initial_resource_versions": "{...}",
  "response_nonce": "...",
  "error_detail": "{...}"
}

DeltaDiscoveryResponse
{
  "system_version_info": "...",
  "resources": [],
  "type_url": "...",
  "removed_resources": [],
  "nonce": "..."
}
```

目前增量的xDS只有grpc版本的，没有REST版本，要求`response_nonce`字段必须成对出现，而全量xDS则不需要，`system_version_info`字段用于debug目的。

`DeltaDiscoveryRequest`用于几个场景:

1. xDS建立后发起的第一次请求
2. 作为ACK/NACK对于前一个`DeltaDiscoveryResponse`进行响应，其response_nonce的值为前一个`DeltaDiscoveryResponse`中的值，至于是ACK还是NACK取决于`error_detail`是否存在
3. client主动发起`DeltaDiscoveryRequest`请求，用于动态添加和移除跟踪的资源，在这种情况下`response_nonce`必须为空

> 哪些情况需要动态添加和移除跟踪的资源?

当发生连接段掉的情况下，客户端重新连接后需要告知自己所拥有的资源名(initial_resource_versions)，当客户端不再对某些资源感兴趣的时候需要在`resource_names_unsubscribe`中列出来



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


## Envoy internal header



## Load balancer

**实现增量更新的思路**:

默认Config Server获取到Host包含了ip、所在机房、单元、machine group、权重还有一些metadata等，都是机器纬度的信息，除了这些信息外，Envoy还需要优先级信息、机器健康状况等
目前可以默认使用优先级0代替 。除了这些机器纬度的信息外，还需要有服务纬度的信息(也可以称为集群纬度的信息)，比如区域权重(每个区域的权重)、过载因子、Endpoint过期时间等。目前这个部分可以不实现，
等开始支持这类功能的时候再单独通过Config Server的SDK通知我这类元信息发生了改变。这个时候就可以更新Locality weight，修改`PriorityState`以及过载因子等。这个时候就需要触发forch rebuild(重新建立LB)。


1. 判断host是新增还是updated
2. 如果是update则先清除`PENDING_DYNAMIC_REMOVAL`，因为马上要加回来了。
3. 判断是否需要原地更新 (有健康检查、并且健康检查的地址不同，这个时候不能跳过原地更新，必须要更新)
4. 如果是原地更新
  1. 判断健康状态是否发生了变化，有变化的话都会导致forch rebuild
  2. metadata变化也需要导致forch rebuild(config server最好直接告知我metadata发生了变化)，并更新host的metadata
  3. 优先级发生了改变，需要更新对应机器的优先级，然后把这个机器添加到当前优先级的列表中

5. 如果不是原地更新
  1. 添加host到当前优先级
  2. 如果健康检查开启的话，先设置这个机器为FAILED_ACTIVE_AC
  3. 然后看是否是warmHosts，如果是就设置为PENDING_ACTIVE_HC

一个主机发生变化了，有几种情况:

1. 新ip的添加
2. 老ip的删除
3. 机房、单元等locality发生了变化 (这个不考虑，这个如果发生了变化会产生添加ip和删除ip两个事件)
4. 健康状态发生了变化
5. 优先级发生了变化
6. 元信息发生了变化

> 健康状态、优先级、元信息发生变化更新host即可


Config Server Cluster计算逻辑:

1. 按照优先级将added_host和removed_hosts以优先级为纬度分成一个个host vector
2. 取出一个优先级往all_host_中查找如果是新添加的就放到hosts_added_to_current_priority中(并执行healthy check相关的flag设置)
   如果不是新的host那就看是否做原地更新，如果是原地更新就直接更新就好了，否则就把这个机器放到hosts_added_to_current_priority中
   如果发生了优先级改变，那么不光光要更新机器的优先级还需要将这个机器添加到hosts_added_to_current_priority列表中，然后对应的要从
   这个机器原来所在优先级中进行删除。(这个我们占时不处理，对于增量的场景，可以简单的将修改前的优先级的机器放到removed列表中)

3. 更新host_change，如果都是原地更新，那么很自然host_change为false、优先级发生变化了也是为true、metadata变化了也是true
   需要将所有host_change为true的场景梳理出来，用于rebuild lb。

4. 拿到当前优先级的host，将hosts_added_to_current_priority追加到当前优先级的列表中，另外需要考虑一个特殊的case:
  host是存在的，但是健康检查地址发生了改变，那么这个机器需要对当前优先级的host做替换而不是追加了。

5. 调用updateClusterPrioritySet进行更新。

6. 如果locality weight发生了改变就设置host_change为true


> updateClusterPrioritySet仍然是全量host进行计算(计算出per locality的结构)，能否再度优化? TODO
> LocalityWeightMap发生改变需要rebuild，这个如何识别出来? Config Server SDK对于locality weight的变化单独通知，然后我来更新PriorityState结构


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






## xDS resource patching

目前endpoint整体是一个resource，xDS的增量只能是resource粒度的增加和减少，对于endpoint来说，他整体就是一个resource，做不了增量的加和减少，因此社区提出了xDS resource patching的机制
可以针对一个resource内部做merge、add、remove、modtify等操作，目前还在review中。

https://github.com/envoyproxy/envoy/issues/8400

## rebalance

用于在多个worker线程之间平衡连接数，目前使用的epoll的方式内核的分配策略是LIFO，会导致连接会分配到固定的前几个worker线程中，这个在大多数情况是OK的，但是对于envoy egress http2
这种少量长连接的场景来说并不是太适合，这种场景连接数并不多，但是连接上的负载很大。

* https://github.com/envoyproxy/envoy/pull/8422
* https://blog.cloudflare.com/the-sad-state-of-linux-socket-balancing/


## HTTP dynamic forward proxy

目前Envoy是支持dynamic forward(istio使用这个能力做egress)，这是通过iptables来实现的，通过iptables保留实际要访问的真实ip(也可以是通过http的header x-envoy-original-dst-host 来传递)，然后通过这个ip动态添加到cluster中，但这个方案并不通用依赖iptables，HTTP dynamic forward proxy则是使用http host header进行动态dns解析，然后通过新增的cluster type(dynamic_forward_proxy)来实现将动态解析的DNS结果放到集群中并把解析结果缓存起来，当第一次请求的时候没有对应的DNS解析结果时，会通过新增的http dynamic forward proxy filter中断当前的请求，然后异步进行DNS解析，等解析完成后再继续转发请求。后续请求进来的时候，直接使用DNS的缓存结果进行请求转发。

https://github.com/envoyproxy/envoy/pull/7307

## on-demand VHDS

`RouteConfiguration`下面包含了`route.VirtualHost`，`route.VirtualHost`里面再包含Route和Action，istio创建virtual service的时候，会导致整个`RouteConfiguration`都发生更新，实际只是新增一个`VirtualHost`或者，是更新一个`VirtualHost`条目。通过VHDS可以将`RouteConfiguration`和`VirtualHost`解耦。结合差量更新还可以做到按需更新。此外这里的on-demand指的是当发起请求的时候，会通过on-demand filter去服务端查询对应的路由条目，然后合并到`RouteConfiguration`中，而不是一次性全下发下来。

https://github.com/envoyproxy/envoy/pull/8617


## stats symbol优化

引入symbol tables，将一个metrics按照"."号分割成多个部分，每一个部分进行编码存在symbol table中，并分配一个symbol id，重复引用相同的部分仅需要保存symbol id，通过这个id来找到对应的字符串。

https://github.com/envoyproxy/envoy/pull/5321


## wasm

目前wasm主要是google的人在主导，有一个独立的仓库envoy-wasm，目前istio已经在使用这个仓库做一些实验性的功能，将mixer的部分能力通过wasm的方式来提供，与此同时wasm的代码也在同步提交到
Envoy官方社区，一旦全部合并进去，istio会将仓库重新指向envoy官方昂哭。

https://github.com/envoyproxy/envoy/pulls?utf8=%E2%9C%93&q=wasm
https://istio.io/docs/ops/telemetry/in-proxy-service-telemetry/


## adaptive concurrency

参考Netflix’s concurrency limits Java library.的算法实现了一个七层的http filter，用来做自适应的并发限流。


https://github.com/envoyproxy/envoy/pull/8582
https://github.com/envoyproxy/envoy/issues/7789
https://github.com/Netflix/concurrency-limits


The adaptive concurrency filter dynamically adjusts the allowed number of requests that can be
outstanding (concurrency) to all hosts in a given cluster at any time. Concurrency values are
calculated using latency sampling of completed requests and comparing the measured samples in a time
window against the expected latency for hosts in the cluster.

一个http filter，用于动态调整允许发出去的请求，并发度通过对已完成请求的延迟采样和一个时间窗口中测量的样本进行比较，群集中主机的预期延迟。

Gradient Controller: 梯度控制器根据定期测量的理想round-trip时间
Concurrency Controllers:  实现了一个算法，用于对每一个请求负责转发的决策，并记录延迟用来计算并发限制





## Grpc & xDS

从最上层开始分析(以cds为例):

1. cds_api创建订阅器

```C++
class CdsApiImpl : public CdsApi,
                   Config::SubscriptionCallbacks,
                   Logger::Loggable<Logger::Id::upstream> {

 private:
  std::unique_ptr<Config::Subscription> subscription_;
}

CdsApiImpl::CdsApiImpl(const envoy::config::core::v3alpha::ConfigSource& cds_config,
                       ClusterManager& cm, Stats::Scope& scope,
                       ProtobufMessage::ValidationVisitor& validation_visitor)
    : cm_(cm), scope_(scope.createScope("cluster_manager.cds.")),
      validation_visitor_(validation_visitor) {
  // 通过SubscriptionFactory来创建的
  subscription_ = cm_.subscriptionFactory().subscriptionFromConfigSource(
      cds_config, loadTypeUrl(cds_config.resource_api_version()), *scope_, *this);
}
```


返回的是一个`Subscription`，CDS可以通过这个接口设置和更新要订阅的资源名

```C++
class Subscription {
public:
  virtual ~Subscription() = default;

  /**
   * Start a configuration subscription asynchronously. This should be called once and will continue
   * to fetch throughout the lifetime of the Subscription object.
   * @param resources set of resource names to fetch.
   */
  virtual void start(const std::set<std::string>& resource_names) PURE;

  /**
   * Update the resources to fetch.
   * @param resources vector of resource names to fetch. It's a (not unordered_)set so that it can
   * be passed to std::set_difference, which must be given sorted collections.
   */
  virtual void updateResourceInterest(const std::set<std::string>& update_to_these_names) PURE;
};
```

实际上是创建了`GrpcMuxSubscriptionImpl`，并将`CdsApiImpl`传给`GrpcMuxSubscriptionImpl`通过`SubscriptionCallbacks`将订阅到的内容回调给`CdsApiImpl`

```C++
  case envoy::config::core::v3alpha::ConfigSource::ConfigSourceSpecifierCase::kAds: {
    if (cm_.adsMux()->isDelta()) {
      result = std::make_unique<DeltaSubscriptionImpl>(
          cm_.adsMux(), type_url, callbacks, stats,
          Utility::configSourceInitialFetchTimeout(config), true);
    } else {
      result = std::make_unique<GrpcMuxSubscriptionImpl>(
          cm_.adsMux(), callbacks, stats, type_url, dispatcher_,
          Utility::configSourceInitialFetchTimeout(config));
    }
    break;
  }
```

SubscriptionCallbacks继承了`SubscriptionCallbacks`和`Subscription`

```C++

class GrpcMuxSubscriptionImpl : public Subscription,
                                GrpcMuxCallbacks,
                                Logger::Loggable<Logger::Id::config> {
  private:
    GrpcMuxSharedPtr grpc_mux_;
    SubscriptionCallbacks& callbacks_;
    GrpcMuxWatchPtr watch_{};
}
```

GrpcMuxCallbacks本质上和SubscriptionCallbacks是一样的，`GrpcMuxSubscriptionImpl`通过底层的`GrpcMuxSharedPtr`(它会回调GrpcMuxCallbacks把收到的内容返回给GrpcMuxSubscriptionImpl)
GrpcMuxSubscriptionImpl再调用`SubscriptionCallbacks`把收到的内容透传给上层的CDS API。

```C++
void GrpcMuxSubscriptionImpl::onConfigUpdate(
    const Protobuf::RepeatedPtrField<ProtobufWkt::Any>& resources,
    const std::string& version_info) {
  .......
  callbacks_.onConfigUpdate(resources, version_info);
  .......
}
```

接下来看下这个`GrpcMuxWatchPtr watch_{};`，它是通过`grpc_mux_`创建的。可以通过`GrpcMuxWatch`来取消订阅。

```C++

watch_ = grpc_mux_->subscribe(type_url_, resources, *this);

/**
 * Handle on an muxed gRPC subscription. The subscription is canceled on destruction.
 */
class GrpcMuxWatch {
public:
  virtual ~GrpcMuxWatch() = default;
};
```

最后就剩下最为核心的`grpc_mux_`了，它是通过`cm_.adsMux()`创建出来的。是ClusterManagerImpl的成员，看起来是所有上层的xDS订阅是复用同一个`GrpcMuxSharedPtr`了


```C++
class ClusterManagerImpl : public ClusterManager, Logger::Loggable<Logger::Id::upstream> {
  public:
    Config::GrpcMuxSharedPtr adsMux() override { return ads_mux_; }
  private:
    Config::GrpcMuxSharedPtr ads_mux_;
}

      ads_mux_ = std::make_shared<Config::GrpcMuxImpl>(
          local_info,
          Config::Utility::factoryForGrpcApiConfigSource(*async_client_manager_,
                                                         dyn_resources.ads_config(), stats)
              ->create(),
          main_thread_dispatcher,
          *Protobuf::DescriptorPool::generated_pool()->FindMethodByName(
              dyn_resources.ads_config().transport_api_version() ==
                      envoy::config::core::v3alpha::ApiVersion::V3ALPHA
                  ? "envoy.service.discovery.v3alpha.AggregatedDiscoveryService."
                    "StreamAggregatedResources"
                  : "envoy.service.discovery.v2.AggregatedDiscoveryService."
                    "StreamAggregatedResources"),
          random_, stats_,
          Envoy::Config::Utility::parseRateLimitSettings(dyn_resources.ads_config()),
          bootstrap.dynamic_resources().ads_config().set_node_on_first_message_only());
```

其实现包含了多个，可能是增量实现NewGrpcMuxImpl、也有可能是全量实现GrpcMuxImpl、或者是空实现NullGrpcMuxImpl，我们主要看下`GrpcMuxImpl`的实现

```C++
class GrpcMuxImpl
    : public GrpcMux,
      public GrpcStreamCallbacks<envoy::service::discovery::v3alpha::DiscoveryResponse>,
      public Logger::Loggable<Logger::Id::config> {
 private:
   GrpcStream<envoy::service::discovery::v3alpha::DiscoveryRequest,
             envoy::service::discovery::v3alpha::DiscoveryResponse>
      grpc_stream_;

}
```

核心是grpc_stream_，通过GrpcStream可以发送、建立stream流

`GrpcStream` Grpc中的一个流是对`Grpc::AsyncStream`的封装，继承自`Grpc::AsyncStreamCallbacks`，提供了一个` void onReceiveMessage(std::unique_ptr<ResponseProto>&& message) override`接口
用于返回收到的message，组合了`Grpc::AsyncClient`用来创建`Grpc::AsyncStream`，提供了sendMessage来发送`stream`，同时也提供了`onReceiveMessage`，当收到message后，回调`GrpcStreamCallbacks`的onDiscoveryResponse方法




`Grpc::AsyncStreamCallbacks` 提供了`onReceiveMessage`的回调，通过`Grpc::AsyncStream`发出message后，通过这个callback获得返回的消息


`Grpc::AsyncClient` grpc Client 来创建一个`Grpc::AsyncStream`

`GrpcStreamCallbacks`，提供了几个和stream相关的callback

```C++
template <class ResponseProto> class GrpcStreamCallbacks {
public:
  virtual ~GrpcStreamCallbacks() = default;

  /**
   * For the GrpcStream to prompt the context to take appropriate action in response to the
   * gRPC stream having been successfully established.
   */
  virtual void onStreamEstablished() PURE;

  /**
   * For the GrpcStream to prompt the context to take appropriate action in response to
   * failure to establish the gRPC stream.
   */
  virtual void onEstablishmentFailure() PURE;

  /**
   * For the GrpcStream to pass received protos to the context.
   */
  virtual void onDiscoveryResponse(std::unique_ptr<ResponseProto>&& message) PURE;

  /**
   * For the GrpcStream to call when its rate limiting logic allows more requests to be sent.
   */
  virtual void onWriteable() PURE;
};
```

`GrpcSubscriptionImpl`

`Config::Subscription` 所有订阅的抽象接口，提供了start和updateResourceInterest来更新资源的订阅，目前有几个实现:

1. `GrpcSubscriptionImpl` 对应xDS
2. `GrpcMuxSubscriptionImpl` 对应ADS
3. `DeltaSubscriptionImpl` 增量xDS
4. `GrpcMuxSubscriptionImpl` 增量ADS
5. `HttpSubscriptionImpl` REST


`SubscriptionCallbacks` 提供`onConfigUpdate`、`onConfigUpdateFailed`、`resourceName`等回调，每一个xDS类型都继承这个callback用来接收配置更新的通知

`GrpcMux` 用来管理单个stream上的多个订阅的，典型的就是ADS stream上处理EDS、CDS、LDS等订阅，主要是提供了start、pause、resume、addOrUpdateWatch等等

`GrpcMuxWatch`

`GrpcMuxWatchImpl` 继承 `GrpcMuxWatch` 每订阅一次资源就创建一个`GrpcMuxWatchImpl`，析构的时候可以用来进行cancel资源订阅。

`GrpcMuxCallbacks` 核心的ADS回调，用于对接收到的xDS资源进行对应的回调，核心接口是:

```C++
/**
 * A grouping of callbacks that a GrpcMux should provide to its GrpcStream.
 */
class GrpcMuxCallbacks {
public:
  virtual ~GrpcMuxCallbacks() = default;

  /**
   * Called when a configuration update is received.
   * @param resources vector of fetched resources corresponding to the configuration update.
   * @param version_info update version.
   * @throw EnvoyException with reason if the configuration is rejected. Otherwise the configuration
   *        is accepted. Accepted configurations have their version_info reflected in subsequent
   *        requests.
   */
  virtual void onConfigUpdate(const Protobuf::RepeatedPtrField<ProtobufWkt::Any>& resources,
                              const std::string& version_info) PURE;

  /**
   * Called when either the subscription is unable to fetch a config update or when onConfigUpdate
   * invokes an exception.
   * @param reason supplies the update failure reason.
   * @param e supplies any exception data on why the fetch failed. May be nullptr.
   */
  virtual void onConfigUpdateFailed(Envoy::Config::ConfigUpdateFailureReason reason,
                                    const EnvoyException* e) PURE;

  /**
   * Obtain the "name" of a v2 API resource in a google.protobuf.Any, e.g. the route config name for
   * a RouteConfiguration, based on the underlying resource type.
   */
  virtual std::string resourceName(const ProtobufWkt::Any& resource) PURE;
};
```

等于SubscriptionCallbacks，两者是一致的，含义相同。



```C++
class Subscription {
public:
  virtual ~Subscription() = default;

  /**
   * Start a configuration subscription asynchronously. This should be called once and will continue
   * to fetch throughout the lifetime of the Subscription object.
   * @param resources set of resource names to fetch.
   */
  virtual void start(const std::set<std::string>& resource_names) PURE;

  /**
   * Update the resources to fetch.
   * @param resources vector of resource names to fetch. It's a (not unordered_)set so that it can
   * be passed to std::set_difference, which must be given sorted collections.
   */
  virtual void updateResourceInterest(const std::set<std::string>& update_to_these_names) PURE;
};

class SubscriptionCallbacks {
public:
  virtual ~SubscriptionCallbacks() = default;
  virtual void onConfigUpdate(const Protobuf::RepeatedPtrField<ProtobufWkt::Any>& resources,
                              const std::string& version_info) PURE;
  virtual void
  onConfigUpdate(const Protobuf::RepeatedPtrField<envoy::service::discovery::v3alpha::Resource>&
                     added_resources,
                 const Protobuf::RepeatedPtrField<std::string>& removed_resources,
                 const std::string& system_version_info) PURE;
  virtual void onConfigUpdateFailed(ConfigUpdateFailureReason reason, const EnvoyException* e) PURE;
  virtual std::string resourceName(const ProtobufWkt::Any& resource) PURE;
};
```

所有的上层API(LDS/CDS/EDS/..)等都继承了`SubscriptionCallbacks`用于获取订阅到的数据。
GrpcMuxSubscriptionImpl继承GrpcMuxCallbacks和Subscription可以给上层API(CDS/LDS/EDS...)等提供资源订阅的接口(Subscription)来订阅资源
每一类资源都需要有一个GrpcMuxSubscriptionImpl类，这个类负责提供资源订阅、更新资源等，还有将订阅的内容回调给上层API


```c++
class SubscriptionFactory {
public:
  virtual ~SubscriptionFactory() = default;

  /**
   * Subscription factory interface.
   *
   * @param config envoy::api::v2::core::ConfigSource to construct from.
   * @param type_url type URL for the resource being subscribed to.
   * @param scope stats scope for any stats tracked by the subscription.
   * @param callbacks the callbacks needed by all Subscription objects, to deliver config updates.
   *                  The callbacks must not result in the deletion of the Subscription object.
   * @return SubscriptionPtr subscription object corresponding for config and type_url.
   */
  virtual SubscriptionPtr
  subscriptionFromConfigSource(const envoy::config::core::v3alpha::ConfigSource& config,
                               absl::string_view type_url, Stats::Scope& scope,
                               SubscriptionCallbacks& callbacks) PURE;
};


  case envoy::config::core::v3alpha::ConfigSource::ConfigSourceSpecifierCase::kAds: {
    if (cm_.adsMux()->isDelta()) {
      result = std::make_unique<DeltaSubscriptionImpl>(
          cm_.adsMux(), type_url, callbacks, stats,
          Utility::configSourceInitialFetchTimeout(config), true);
    } else {
      result = std::make_unique<GrpcMuxSubscriptionImpl>(
          cm_.adsMux(), callbacks, stats, type_url, dispatcher_,
          Utility::configSourceInitialFetchTimeout(config));
    }
    break;
  }
```

针对每一个type_url的资源都创建一个订阅器，等待内容回调。实际订阅器就是GrpcMuxSubscriptionImpl对象，接下来我们看下`GrpcMuxSubscriptionImpl`对象

```c++
class GrpcMuxSubscriptionImpl : public Subscription,
                                GrpcMuxCallbacks,
                                Logger::Loggable<Logger::Id::config> {
public:
  GrpcMuxSubscriptionImpl(GrpcMuxSharedPtr grpc_mux, SubscriptionCallbacks& callbacks,
                          SubscriptionStats stats, absl::string_view type_url,
                          Event::Dispatcher& dispatcher,
                          std::chrono::milliseconds init_fetch_timeout);

  // Config::Subscription
  void start(const std::set<std::string>& resource_names) override;
  void updateResourceInterest(const std::set<std::string>& update_to_these_names) override;

  // Config::GrpcMuxCallbacks
  void onConfigUpdate(const Protobuf::RepeatedPtrField<ProtobufWkt::Any>& resources,
                      const std::string& version_info) override;
  void onConfigUpdateFailed(Envoy::Config::ConfigUpdateFailureReason reason,
                            const EnvoyException* e) override;
  std::string resourceName(const ProtobufWkt::Any& resource) override;

private:
  void disableInitFetchTimeoutTimer();

  GrpcMuxSharedPtr grpc_mux_;
  SubscriptionCallbacks& callbacks_;
  SubscriptionStats stats_;
  const std::string type_url_;
  GrpcMuxWatchPtr watch_{};
  Event::Dispatcher& dispatcher_;
  std::chrono::milliseconds init_fetch_timeout_;
  Event::TimerPtr init_fetch_timeout_timer_;
};
```

组合了GrpcMuxSharedPtr(NewGrpcMuxImpl)增量实现、或者(GrpcMuxImpl)全量实现
`GrpcMuxImpl` 通过调用底层的`GrpcStream`来发送stream，同时继承了`GrpcStreamCallbacks`用来接收stream返回的message



```C++
class GrpcMuxImpl
    : public GrpcMux,
      public GrpcStreamCallbacks<envoy::service::discovery::v3alpha::DiscoveryResponse>,
      public Logger::Loggable<Logger::Id::config> {

  private:
    GrpcStream<envoy::service::discovery::v3alpha::DiscoveryRequest,
             envoy::service::discovery::v3alpha::DiscoveryResponse>
      grpc_stream_;
}


class GrpcStream : public Grpc::AsyncStreamCallbacks<ResponseProto>,
                   public Logger::Loggable<Logger::Id::config> {
  private:
    Grpc::AsyncClient<RequestProto, ResponseProto> async_client_;
    Grpc::AsyncStream<RequestProto> stream_{};
}
```

```C++
/**
 * Manage one or more gRPC subscriptions on a single stream to management server. This can be used
 * for a single xDS API, e.g. EDS, or to combined multiple xDS APIs for ADS.
 */
class GrpcMux {
public:
  virtual ~GrpcMux() = default;
  virtual void start() PURE;
  virtual GrpcMuxWatchPtr subscribe(const std::string& type_url,
                                    const std::set<std::string>& resources,
                                    GrpcMuxCallbacks& callbacks) PURE;
  virtual void pause(const std::string& type_url) PURE;
  virtual void resume(const std::string& type_url) PURE;
  virtual bool isDelta() const PURE;

  // For delta
  virtual Watch* addOrUpdateWatch(const std::string& type_url, Watch* watch,
                                  const std::set<std::string>& resources,
                                  SubscriptionCallbacks& callbacks,
                                  std::chrono::milliseconds init_fetch_timeout) PURE;
  virtual void removeWatch(const std::string& type_url, Watch* watch) PURE;
  virtual bool paused(const std::string& type_url) const PURE;
};

```

## InitManager

`WatcherHandle`、`Mannager`、`Target`、`TargetHandle`、`Watcher`、`WatcherHandle`

一个WatcherImpl持有一个ReadyFn的回调，通过WatcherImpl可以创建WatcherHandle持有对ReadyFn的弱回调。
TargetImpl和TargetHandleImpl和WatcherImpl和WatcherHandle类似，只是前者是持有ReadyFn后者是持有InternalInitalizeFn



## grpc client

基本概念解释:

* `AsyncRequest` An in-flight gRPC unary RPC. 单向的grpc请求，可能正在发送中，可以cancel掉
* `RawAsyncStream`  An in-flight gRPC stream. 通过grpc stream来发送请求
* `RawAsyncRequestCallbacks`
* `RawAsyncStreamCallbacks`
* `RawAsyncClient`  发送grpc请求，异步接收响应



* `AsyncClientImpl`
继承自RawAsyncClient，具备发送单向grpc消息，返回一个AsyncRequest，可以对这个请求做cancel，response通过RawAsyncRequestCallbacks返回
也可以发送grpc stream，返回一个RawAsyncStream，一个stream，通过这个stream可以发送消息，response通过RawAsyncStreamCallbacks返回





## Envoy 性能优化




## Http filter status

分为两类:

* FilterHeadersStatus
  1. Continue
  2. StopIteration                  // 停止对下面的filter进行iterate，除非主动调用continueDecoding()/continueEncoding()来继续
  3. ContinueAndEndStream           // 继续iterate下面的filter，但是忽略后续的data和trailer
  4. StopAllIterationAndBuffer      // 停止iterate下面的filter，但是会继续buffer后续的data，达到buffer的限制后会返回401或500
  5. StopAllIterationAndWatermark   // 停止iterate下面的filter，但是会继续buffer后续的data，直到触发high watermark

* FilterDataStatus
  1. Continue
  2. StopIterationAndBuffer
  3. StopIterationAndWatermark
  4. StopIterationNoBuffer

* FilterTrailersStatus
  1. Continue
  2. StopIteration

* FilterMetadataStatus
  1. Continue





  ## TLS证书


  ```yaml
  static_resources:
  listeners:
  - address:
      socket_address:
        address: 0.0.0.0
        port_value: 443
    filter_chains:
    - filters:
      - name: envoy.filters.network.http_connection_manager
        typed_config:
          "@type": type.googleapis.com/envoy.config.filter.network.http_connection_manager.v2.HttpConnectionManager
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
          http_filters:
          - name: envoy.filters.http.router
            typed_config: {}
      # 服务端验证
      transport_socket:
        name: envoy.transport_socket.tls
        typed_config:
          "@type": type.googleapis.com/envoy.api.v2.auth.DownstreamTlsContext
          common_tls_context:
            # 证书相关的信息
            tls_certificates:
            # 证书
            - certificate_chain: { filename: "/root/gateway/api.alimesh.alibaba-inc.com_SHA256withRSA_EC.crt" }
            # 私钥
              private_key: { filename: "/root/gateway/api.alimesh.alibaba-inc.com_SHA256withRSA_EC.key" }
              # 客户端验证
            validation_context:
              trusted_ca:
                # 验证客户端证书的root ca，一般用系统的就足够
                filename: "/etc/ssl/certs/ca-certificates.crt"
                # 验证模式(Peer certificate verification mode.)，默认客户端证书必须是经过指定CA列表中的CA进行验证的
              trust_chain_verification: ACCEPT_UNTRUSTED
          require_client_certificate: false
  clusters:
  - name: service1
    connect_timeout: 0.25s
    type: strict_dns
    lb_policy: round_robin
    transport_socket:
      name: envoy.transport_socket.tls
      typed_config:
          "@type": type.googleapis.com/envoy.api.v2.auth.UpstreamTlsContext
          CommonTlsContext:
            validation_context:
              trust_chain_verification: ACCEPT_UNTRUSTED
    load_assignment:
      cluster_name: service1
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: 9999
admin:
  access_log_path: "/dev/null"
  address:
    socket_address:
      address: 0.0.0.0
      port_value: 8001
  ```



  ## xDS transport next steps

  1. Collections: 没有一种通用机制来选择资源子集，大家现在普遍的做法是通过在Node中添加一些信息让控制面可以进行计算子集。但是这种方式不通用。Envoy希望引入namesapce机制来选择子集
     更通用的方法可以是引入一个key/value属性集合去选择请求的资源，对于增量xDS来说也是有意义的。

Ref:
  https://github.com/envoyproxy/envoy/pull/10689
  https://github.com/envoyproxy/envoy/issues/10373

  2. Resource immutability: xDS目前没有一个不可以变的资源名到资源的映射关系，目前是控制面结合Node信息来生成或者修改资源名，两个客户端可能看到相同资源名的不同资源，这对于xDS Cache并不优化
     同时这种行为也会影响多控制面的联邦，不同的控制面必须具有一个全局的资源命名方式。

Ref:
  https://docs.google.com/document/d/1X9fFzqBZzrSmx2d0NkmjDW2tc8ysbLQS9LaRQRxdJZU/edit?disco=AAAAGQ_84vU&ts=5e61532c&usp_dm=false


  3. Per-xDS type resource name semantics: VHDS的资源名是一种特殊结构，格式要求为`<route configuration name>/<host entry>`，这允许控制面根据这种格式来查询指定路由配置中virtual host中的entry
     在这种情况下，资源名其实就是扮演了namesapce的角色。在未来其他的资源类型的名字将可能需要某种结构，理想的资源命名方案将允许结构，同时留给控制平面一些自由，以便在需要时继续使用不透明命名，而不是将不透明和结构化形式任意混合在一起。

  4. Flat representation: 现如今，资源标识已成为资源名称，节点信息，ConfigSource的权限，URL类型和版本的大杂烩。
    在无法处理相关proto3消息的人员或系统之间进行通信时，没有标准格式可以对这些信息进行简明的编码。
    URI提供了一种强大的符合标准的方式来指定此信息。未来的xDS资源命名方案将受益于具有规范的URI表示（除了规范的proto3定义）

  5. Implicit resource payload naming
    虽然在增量发现中，我们在DeltaDiscoveryResponse中的“资源”消息中有明确提供的资源名称，但SotW资源有效负载却不是这种情况。这意味着xDS代理，缓存和其他中介必须具有按资源类型的识别和反序列化才能知道正在传递的资源有效负载。我们需要为DiscoveryResponses中的SotW资源命名提供更好的解决方案，可能会弃用现有的重复的Any，并用v3 / v4 xDS中的重复的Resource代替。

  6. Aliasing
    VHDS为xDS引入了别名，因为同一个虚拟主机资源可能会被多个名称（例如， routeConfigA / foo.com，routeConfigA / bar.com）使用。此机制尚未在xDS的其他地方广泛使用，因此用更通用的替代它是理想的。一个有吸引力的替代方案（将在下面的联合讨论中用于其他目的）也将对xDS引入重定向功能。当查询routeConfigA / foo.com或routeConfigA / bar.com时，管理服务器可能会发出重定向，导致xDS客户端获取某些CanonicalVirtualHost资源。通过消除概念上的复杂性，这简化了xDS控制平面的要求。



## 自定义cluster实现的关键点

1. 如果这个cluster需要依赖其他cluster才能获取到地址信息，那么这个cluster的initializa phase需要设置为Secondary，否则是Primary

```cpp
  // Upstream::Cluster
  Upstream::Cluster::InitializePhase initializePhase() const override {
    return Upstream::Cluster::InitializePhase::Primary;
  }
```


2. `cluster`的初始化需要放到`startPreInit`中，初始化完成后需要调用`onPreInitComplete`来完成初始化，最好做一个超时控制

```cpp
  // Upstream::ClusterImplBase
  void startPreInit() override;
```

3. host更新的时候需要按照优先级为纬度来进行更新，并按照优先级纬度统计locality weight map，通过PriorityStateManager来管理


PriorityStateManager 在初始化优先级的时候会自动构建好locality weight map

```cpp
void PriorityStateManager::initializePriorityFor(
    const envoy::api::v2::endpoint::LocalityLbEndpoints& locality_lb_endpoint) {
  const uint32_t priority = locality_lb_endpoint.priority();
  if (priority_state_.size() <= priority) {
    priority_state_.resize(priority + 1);
  }
  if (priority_state_[priority].first == nullptr) {
    priority_state_[priority].first = std::make_unique<HostVector>();
  }
  if (locality_lb_endpoint.has_locality() && locality_lb_endpoint.has_load_balancing_weight()) {
    priority_state_[priority].second[locality_lb_endpoint.locality()] =
        locality_lb_endpoint.load_balancing_weight().value();
  }
}
```

4. 计算host变更(增加多少、减少多少，发生变化)，如果没有host变更，但是其他的一些信息发生了变更也需要进行lb重建

```cpp
 const auto& host_set = priority_set_.getOrCreateHostSet(priority, overprovisioning_factor);
  HostVectorSharedPtr current_hosts_copy(new HostVector(host_set.hosts()));

  HostVector hosts_added;
  HostVector hosts_removed;
  // We need to trigger updateHosts with the new host vectors if they have changed. We also do this
  // when the locality weight map or the overprovisioning factor. Note calling updateDynamicHostList
  // is responsible for both determining whether there was a change and to perform the actual update
  // to current_hosts_copy, so it must be called even if we know that we need to update (e.g. if the
  // overprovisioning factor changes).
  // TODO(htuch): We eagerly update all the host sets here on weight changes, which isn't great,
  // since this has the knock on effect that we rebuild the load balancers and locality scheduler.
  // We could make this happen lazily, as we do for host-level weight updates, where as things age
  // out of the locality scheduler, we discover their new weights. We don't currently have a shared
  // object for locality weights that we can update here, we should add something like this to
  // improve performance and scalability of locality weight updates.
  const bool hosts_updated = updateDynamicHostList(new_hosts, *current_hosts_copy, hosts_added,
                                                   hosts_removed, updated_hosts, all_hosts_);
  if (hosts_updated || host_set.overprovisioningFactor() != overprovisioning_factor ||
      locality_weights_map != new_locality_weights_map) {
    ASSERT(std::all_of(current_hosts_copy->begin(), current_hosts_copy->end(),
                       [&](const auto& host) { return host->priority() == priority; }));
    locality_weights_map = new_locality_weights_map;
    ENVOY_LOG(debug,
              "EDS hosts or locality weights changed for cluster: {} current hosts {} priority {}",
              info_->name(), host_set.hosts().size(), host_set.priority());
    // lb 重建
    priority_state_manager.updateClusterPrioritySet(priority, std::move(current_hosts_copy),
                                                    hosts_added, hosts_removed, absl::nullopt,
                                                    overprovisioning_factor);
    return true;
  }
  return false;
```

1. locality_weight_map发生变化
2. overprovisioning_factor发生变化
3. host发生变更(新增、删除)，host没有变更，但是有一些信息发生了变化)

Host信息发生变化

  1. host优先级变化
  2. host metadata变化
  3. host配置的健康地址发生了变化

这些都会导致lb重建，需要更新priorityset，即使没有任何机器发生变化

5. lb重建和priority set更新

通过priority_state_manager的updateClusterPrioritySet方法可以更新priorityset并重建lb

```cpp
    priority_state_manager.updateClusterPrioritySet(priority, std::move(current_hosts_copy),
                                                    hosts_added, hosts_removed, absl::nullopt,
                                                    overprovisioning_factor);
```


这个方法主要做了几件事:

1. 根据locality weight map还有是否开启zone aware路由来构建LocalityWeights lb
2. 按照locality纬度将机器组织起来，并最终构建HostsPerLocalityImpl
3. 调用priorityset的updateHosts方法更新priority set，并传入计算出来的新增、和删除host用来进行通知
4. 处理健康检查


## 连接池

## FilterChain


FilterChainFactory接口，通过这个接口可以创建Network filter chain、Listener filter chain、Udp listener filter chain

```C++
/**
 * Creates a chain of network filters for a new connection.
 */
class FilterChainFactory {
public:
  virtual ~FilterChainFactory() = default;

  /**
   * Called to create the network filter chain.
   * @param connection supplies the connection to create the chain on.
   * @param filter_factories supplies a list of filter factories to create the chain from.
   * @return true if filter chain was created successfully. Otherwise
   *   false, e.g. filter chain is empty.
   */
  virtual bool createNetworkFilterChain(Connection& connection,
                                        const std::vector<FilterFactoryCb>& filter_factories) PURE;

  /**
   * Called to create the listener filter chain.
   * @param listener supplies the listener to create the chain on.
   * @return true if filter chain was created successfully. Otherwise false.
   */
  virtual bool createListenerFilterChain(ListenerFilterManager& listener) PURE;

  /**
   * Called to create a Udp Listener Filter Chain object
   *
   * @param udp_listener supplies the listener to create the chain on.
   * @param callbacks supplies the callbacks needed to create a filter.
   */
  virtual void createUdpListenerFilterChain(UdpListenerFilterManager& udp_listener,
                                            UdpReadFilterCallbacks& callbacks) PURE;
};
```


## Extension Config Discovery Service

