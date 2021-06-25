# 基本概念和接口

* `labels.Instance` 用于实现label匹配的，典型的实现场景在于`WorkloadSelector`，这是一个map，里面可以填一些key、value，用于匹配符合条件的节点。
* `networking.EnvoyFilter_PatchContext` Inbound、Outbound、Sidecar、Gateway、Any，也就是EnvoyFilter生效的范围。
* `WatchedNamespaces` 默认pilot从所有的namespace中获取服务信息，如果配置了这个参数则只会从指定的namespace中获取服务信息
* `RootNamespace` istio配置的admin namespace，放在这里的配置全局生效
* `ConfigNamespace` Proxy所在的Namespace，由Proxy带上去的。
* `Connection` 一个Client的信息，包含了这个Client的地址、标识符、连接上来的时间、Node信息、Blocked的请求信息等
* `model.PushRequest` 一个Push请求，包含了一次Push相关的信息，是否是Full Push、哪些配置更新了，什么时候开始的，触发的原因等

```go
// PushRequest defines a request to push to proxies
// It is used to send updates to the config update debouncer and pass to the PushQueue.
type PushRequest struct {
	// Full determines whether a full push is required or not. If false, an incremental update will be sent.
	// Incremental pushes:
	// * Do not recompute the push context
	// * Do not recompute proxy state (such as ServiceInstances)
	// * Are not reported in standard metrics such as push time
	// As a result, configuration updates should never be incremental. Generally, only EDS will set this, but
	// in the future SDS will as well.
	Full bool

	// ConfigsUpdated keeps track of configs that have changed.
	// This is used as an optimization to avoid unnecessary pushes to proxies that are scoped with a Sidecar.
	// If this is empty, then all proxies will get an update.
	// Otherwise only proxies depend on these configs will get an update.
	// The kind of resources are defined in pkg/config/schemas.
	ConfigsUpdated map[ConfigKey]struct{}

	// Push stores the push context to use for the update. This may initially be nil, as we will
	// debounce changes before a PushContext is eventually created.
	Push *PushContext

	// Start represents the time a push was started. This represents the time of adding to the PushQueue.
	// Note that this does not include time spent debouncing.
	Start time.Time

	// Reason represents the reason for requesting a push. This should only be a fixed set of values,
	// to avoid unbounded cardinality in metrics. If this is not set, it may be automatically filled in later.
	// There should only be multiple reasons if the push request is the result of two distinct triggers, rather than
	// classifying a single trigger as having multiple reasons.
	Reason []TriggerReason
}
```

* `PushContext` 存储Push时保存的相关的资源信息
