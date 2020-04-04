
## 自定义API


## pilot-discovery


### 基本概念

* `XdsConnection`

每一个client连接上来(并不是连接，是stream，一个ADS stream就是一个`XdsConnection`，在这个ADS stream之上可以发送xDS请求)，每一个`XdsConnection`包含一个XdsEvent的chan
push的时候，会生成一个`model.PushRequest`结构，然后遍历所有的XdsConnection，组合成一个二元组通过`PushQueue`的`Enqueue`放到队列中。


```go
// 包含了一次推送的所有信息
type XdsEvent struct {
	// If not empty, it is used to indicate the event is caused by a change in the clusters.
	// Only EDS for the listed clusters will be sent.
	edsUpdatedServices map[string]struct{}

	namespacesUpdated map[string]struct{}

	configTypesUpdated map[string]struct{}

	// Push context to use for the push.
	push *model.PushContext

	// start represents the time a push was started.
	start time.Time

	// function to call once a push is finished. This must be called or future changes may be blocked.
	done func()

	noncePrefix string
}
```


XDSUpdater接口

```go
type XDSUpdater interface {

	// EDSUpdate is called when the list of endpoints or labels in a ServiceEntry is
	// changed. For each cluster and hostname, the full list of active endpoints (including empty list)
	// must be sent. The shard name is used as a key - current implementation is using the registry
	// name.
	EDSUpdate(shard, hostname string, namespace string, entry []*IstioEndpoint) error

	// SvcUpdate is called when a service port mapping definition is updated.
	// This interface is WIP - labels, annotations and other changes to service may be
	// updated to force a EDS and CDS recomputation and incremental push, as it doesn't affect
	// LDS/RDS.
	SvcUpdate(shard, hostname string, ports map[string]uint32, rports map[uint32]string)

	// ConfigUpdate is called to notify the XDS server of config updates and request a push.
	// The requests may be collapsed and throttled.
	// This replaces the 'cache invalidation' model.
	ConfigUpdate(req *PushRequest)

	// ProxyUpdate is called to notify the XDS server to send a push to the specified proxy.
	// The requests may be collapsed and throttled.
	ProxyUpdate(clusterID, ip string)
}
```

```go
func (p *PushQueue) Enqueue(proxy *XdsConnection, pushInfo *model.PushRequest)
```

然后通过条件变量唤醒PushQueue，唤醒后通过Dequeue取出`XdsConnection`和`model.PushRequest`，最后根据`model.PushRequest`转换成`XdsEvent`，通知给`XdsConnection`

```go
func doSendPushes(stopCh <-chan struct{}, semaphore chan struct{}, queue *PushQueue) {
	for {
		select {
		case <-stopCh:
			return
		default:
			// We can send to it until it is full, then it will block until a pushes finishes and reads from it.
			// This limits the number of pushes that can happen concurrently
			semaphore <- struct{}{}

			// Get the next proxy to push. This will block if there are no updates required.
			client, info := queue.Dequeue()

			// Signals that a push is done by reading from the semaphore, allowing another send on it.
			doneFunc := func() {
				queue.MarkDone(client)
				<-semaphore
			}

			proxiesQueueTime.Record(time.Since(info.Start).Seconds())

			go func() {
				edsUpdates := info.EdsUpdates
				if info.Full {
					// Setting this to nil will trigger a full push
					edsUpdates = nil
				}

				select {
				case client.pushChannel <- &XdsEvent{
					push:               info.Push,
					edsUpdatedServices: edsUpdates,
					done:               doneFunc,
					start:              info.Start,
					namespacesUpdated:  info.NamespacesUpdated,
					configTypesUpdated: info.ConfigTypesUpdated,
					noncePrefix:        info.Push.Version,
				}:
					return
				case <-client.stream.Context().Done(): // grpc stream was closed
					doneFunc()
					adsLog.Infof("Client closed connection %v", client.ConID)
				}
			}()
		}
	}
}
```



### 服务变更的过程

// 服务变更会导致kube register触发事件，DiscoverServer注册了事件回调，导致一个全量更新的PushRequest请求

```go
	// Flush cached discovery responses whenever services configuration change.
	serviceHandler := func(svc *model.Service, _ model.Event) {
		pushReq := &model.PushRequest{
			Full:               true,
			NamespacesUpdated:  map[string]struct{}{svc.Attributes.Namespace: {}},
			ConfigTypesUpdated: map[string]struct{}{schemas.ServiceEntry.Type: {}},
		}
		out.ConfigUpdate(pushReq)
	}
	if err := ctl.AppendServiceHandler(serviceHandler); err != nil {
		adsLog.Errorf("Append service handler failed: %v", err)
		return nil
  }

  // ConfigUpdate implements ConfigUpdater interface, used to request pushes.
// It replaces the 'clear cache' from v1.
func (s *DiscoveryServer) ConfigUpdate(req *model.PushRequest) {
	inboundConfigUpdates.Increment()
	s.pushChannel <- req
}
```

### 全量更新的过程


### EDS增量更新的过程




## 前置条件

1. Named ports: 定义service的时候，其中port字段需要使用`<protocol>[-<suffix>]`的形式，其中protocol需要是http、http2、grpc、mongo、redis等目的是为了使用istio的路由特性，否则会被认为是TCP流量。
2. Service association: 一个POD如果关联多个service的时候，这个service不能是相同的port但却是不同的协议
3. Deployments with app and version labels: pods部署的时候使用Kubernetes Deployment需要显示的打上app和version的标签。这些标签用于分布式tracing和metric telemetry采集。


## Gateway

Gateway的两种模式:

1. standard
2. sni-dnat


istio的gateway为什么要配置成这样?

```
"max_requests_per_connection": 1,
```