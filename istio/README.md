

## 核心数据结构

```go
type Environment struct {
}
```

给Pilot提供了一个聚合的API环境，将服务发现、配置list、mesh config聚合在一起了

```go
// PushContext tracks the status of a push - metrics and errors.
// Metrics are reset after a push - at the beginning all
// values are zero, and when push completes the status is reset.
// The struct is exposed in a debug endpoint - fields public to allow
// easy serialization as json.
type PushContext struct {
}
```

ClusterID: istiod所在集群的ID，可以通过参数指定，如果是在k8s环境下就等于k8s的ServiceRegistry名字，其他环境下这个值为0。
这个字段的目的是为了实现多集群。