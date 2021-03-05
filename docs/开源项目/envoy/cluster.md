# Cluster分析

在Envoy中`Cluster`表示的是一个集群，一个集群主要包含两个部分的信息，一个部分是这个集群相关的配置，比如集群的名字、集群下机器建链的超时时间、负载均衡策略、建立链接用什么协议等等。
另外一个部分则是这个集群下所包含的机器列表。例如下面这个例子。

```yaml
  clusters:
  - name: statsd
    type: STATIC
    connect_timeout: 0.25s
    lb_policy: ROUND_ROBIN
    load_assignment:
      cluster_name: statsd
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: 8125
                protocol: TCP
```

上面这段yaml定义了一个名为statsd的集群，负载均衡策略是`ROUND_ROBIN`、连接超时时间是`0.25s`、这个集群下面有一个机器，这个集群的类型是`STATIC`。根据这段`yaml`Envoy就会创建一个`Cluster`对象。
这个Cluster对象并非是一个通用的对象，而且根据yaml中的type字段，找到对象类型的Cluster的构造工厂函数来进行构造。


而STRICT_DNS类型的Cluster则是通过DNS查询指定域名来获取机器列表的。而EDS类型的Cluster则是通过发送EDS请求给控制面来获取机器列表的。无论是何种方式获取，
最终机器列表都是存在`envoy::config::endpoint::v3::ClusterLoadAssignment`这样的protobuf message中的。



一个集群下面是多个`LocalityLbEndpoints`，一个`LocalityLbEndpoints`包含一个`Locality`、一个优先级、一个区域的权重、以及一批`LbEndpoint`
一个`LbEndpoint`包含了一个机器和对应的元数据和权重。

一个集群下面可以配置多个Priority和多个Locality，每一个Priority和Locality可以包含一组endpoint，Priority和Locality是多对多的关系
也就是说一个Priority可以包含多个Locality的机器列表，一个Locality也可以包含多个priority，当一个priority中的主机不可用/不健康的时候
会去从下一个priority中进行选择。


一个集群包含一个`PrioritySetImpl`，包含了一个集群下的所有host，然后按照`priority`维度对给定集群进行分组，一个集群会有多个`Priority`


一个`PrioritySetImpl`里面就是一个vector，索引就是优先级，内容就是这个优先级下对应的`HostSetImpl`
`PrioritySetImpl` =>  `std::vector<std::unique_ptr<HostSet>> host_sets_;`


一个`HostSetImpl` 就是按照priority来维护一组host集合，构造函数需要提供priority，然后通过updateHosts来添加host。这组host可能来自于多个`Locality`
`HostSetImpl`会根据`degraded`、`healthy`、`excluded`等唯独将Host分类，然后针对每一个分类将Host按照`Locality`进行分类，也就是通过`HostsPerLocalityImpl`来保存

```cpp
uint32_t priority_;
uint32_t overprovisioning_factor_;
HostVectorConstSharedPtr hosts_;
HealthyHostVectorConstSharedPtr healthy_hosts_;
DegradedHostVectorConstSharedPtr degraded_hosts_;
ExcludedHostVectorConstSharedPtr excluded_hosts_;
HostsPerLocalityConstSharedPtr hosts_per_locality_{HostsPerLocalityImpl::empty()};
HostsPerLocalityConstSharedPtr healthy_hosts_per_locality_{HostsPerLocalityImpl::empty()};
HostsPerLocalityConstSharedPtr degraded_hosts_per_locality_{HostsPerLocalityImpl::empty()};
HostsPerLocalityConstSharedPtr excluded_hosts_per_locality_{HostsPerLocalityImpl::empty()};
LocalityWeightsConstSharedPtr locality_weights_;
```

一个`HostsPerLocalityImpl` 按照`locality`的维度组织host集合，核心数据成员`std::vector<HostVector>`，第一个host集合是local locality。

```cpp
// The first entry is for local hosts in the local locality.
std::vector<HostVector> hosts_per_locality_;
```

一次host更新需要将所有的host按照优先级、locality、healthy等纬度组装成对应的结构，而`PriorityStateManager`在其中充当了重要的角色。
`PriorityStateManager` 管理一个集群的`PriorityState`，而PriorityState则是按照priority分组维护的一组host和对应的locality weight map，

```cpp
using PriorityState = std::vector<std::pair<HostListPtr, LocalityWeightsMap>>;
using LocalityWeightsMap =
    std::unordered_map<envoy::config::core::v3::Locality, uint32_t, LocalityHash, LocalityEqualTo>;
```

更新host的时候先给这批host生成`PriorityState`，vector的下标是优先级，保存了一个优先级下的所有host，以及这个优先级对应的所有Locality和权重
通过`PriorityStateManager::updateClusterPrioritySet`就可以更新集群的`PrioritySetImpl`的指定优先级下的所有Host。这个函数内部会通过`PriorityState`拿到区域权重
然后将所有的Host按照`Locality`进行组织起来，然后计算出每一个`Locality`的权重。最后调用`PrioritySetImpl::updateHosts`来更新指定优先级的Host。


HostDescriptionImpl 对upstream主机的一个描述接口，比如是否是canary、metadata、cluster、healthChecjer、hostname、address等
HostImpl 代表一个upstream Host
LocalityWeightsMap key是Locality，value是这个Locality的权重