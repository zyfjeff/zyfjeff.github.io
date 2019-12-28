
## pilot-discovery


### 基本概念

* `XdsConnection`

每一个client连接上来(并不是连接，是stream，一个ADS stream就是一个`XdsConnection`，在这个ADS stream之上可以发送xDS请求)，每一个`XdsConnection`包含一个XdsEvent的chan
push的时候，会生成一个`model.PushRequest`结构，然后遍历所有的XdsConnection，组合成一个二元组通过`PushQueue`的`Enqueue`放到队列中。

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




