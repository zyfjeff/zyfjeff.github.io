# 基本概念和接口

* `labels.Instance` 用于实现label匹配的，典型的实现场景在于`WorkloadSelector`，这是一个map，里面可以填一些key、value，用于匹配符合条件的节点。
* `networking.EnvoyFilter_PatchContext` Inbound、Outbound、Sidecar、Gateway、Any，也就是EnvoyFilter生效的范围。
* `WatchedNamespaces` 默认pilot从所有的namespace中获取服务信息，如果配置了这个参数则只会从指定的namespace中获取服务信息
* `RootNamespace` istio配置的admin namespace，放在这里的配置全局生效
* `ConfigNamespace` Proxy所在的Namespace，由Proxy带上去的。