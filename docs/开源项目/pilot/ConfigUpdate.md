---
hide:
  - toc        # Hide table of contents
  - navigation
---

# 配置更新分析 

1. `MeshConfig`、` MeshNetworkConfig` 配置发生变更会触发Full Push

    ```go hl_lines="8-10 14-16"
    // initMeshHandlers initializes mesh and network handlers.
    func (s *Server) initMeshHandlers() {
        log.Info("initializing mesh handlers")
        // When the mesh config or networks change, do a full push.
        s.environment.AddMeshHandler(func() {
            spiffe.SetTrustDomain(s.environment.Mesh().GetTrustDomain())
            s.XDSServer.ConfigGenerator.MeshConfigChanged(s.environment.Mesh())
            s.XDSServer.ConfigUpdate(&model.PushRequest{
                Full:   true,
                Reason: []model.TriggerReason{model.GlobalUpdate},
            })
        })
        s.environment.AddNetworksHandler(func() {
            s.XDSServer.ConfigUpdate(&model.PushRequest{
                Full:   true,
                Reason: []model.TriggerReason{model.GlobalUpdate},
            })
        })
    }
    ```

2. 服务更新，配置更新，最后都会调用ConfigUpdate接口，并传递PushRequest

    ```go hl_lines="6-15 21-30"
    // initRegistryEventHandlers sets up event handlers for config and service updates
    func (s *Server) initRegistryEventHandlers() {
        log.Info("initializing registry event handlers")
        // Flush cached discovery responses whenever services configuration change.
        serviceHandler := func(svc *model.Service, _ model.Event) {
            pushReq := &model.PushRequest{
                Full: true,
                ConfigsUpdated: map[model.ConfigKey]struct{}{{
                    Kind:      gvk.ServiceEntry,
                    Name:      string(svc.Hostname),
                    Namespace: svc.Attributes.Namespace,
                }: {}},
                Reason: []model.TriggerReason{model.ServiceUpdate},
            }
            s.XDSServer.ConfigUpdate(pushReq)
        }
        s.ServiceController().AppendServiceHandler(serviceHandler)

        if s.configController != nil {
            configHandler := func(old config.Config, curr config.Config, event model.Event) {
                ......
                pushReq := &model.PushRequest{
                    Full: true,
                    ConfigsUpdated: map[model.ConfigKey]struct{}{{
                        Kind:      curr.GroupVersionKind,
                        Name:      curr.Name,
                        Namespace: curr.Namespace,
                    }: {}},
                    Reason: []model.TriggerReason{model.ConfigUpdate},
                }
                s.XDSServer.ConfigUpdate(pushReq)
                .....
            }
            .....
        }
    }
    ```


    无论是什么配置，最后的入口都是`ConfigUpdate`

    ```go
    func (s *DiscoveryServer) ConfigUpdate(req *model.PushRequest)
    ```

3. 通过debounce后，最终放入到推送队列中

    ```go
    func (s *DiscoveryServer) ConfigUpdate(req *model.PushRequest) {
        inboundConfigUpdates.Increment()
        s.InboundUpdates.Inc()
        s.pushChannel <- req
    }

    // 通过pushChannel触发debounce
    // debounce最终会触发Push的调用
    func (s *DiscoveryServer) handleUpdates(stopCh <-chan struct{}) {
        debounce(s.pushChannel, stopCh, s.debounceOptions, s.Push, s.CommittedUpdates)
    }

    // Push is called to push changes on config updates using ADS. This is set in DiscoveryService.Push,
    // to avoid direct dependencies.
    // Push的最后会调用AdsPushAll，并携带PushContext
    func (s *DiscoveryServer) Push(req *model.PushRequest) {
        // 非Full Push的场景，直接用全局的PushContext
        if !req.Full {
            req.Push = s.globalPushContext()
            s.AdsPushAll(versionInfo(), req)
            return
        }
        // Full Push场景则用Global Push COntext重新生成新的PushContext
        // Reset the status during the push.
        oldPushContext := s.globalPushContext()
        if oldPushContext != nil {
            oldPushContext.OnConfigChange()
        }
        // PushContext is reset after a config change. Previous status is
        // saved.
        t0 := time.Now()

        versionLocal := time.Now().Format(time.RFC3339) + "/" + strconv.FormatUint(versionNum.Inc(), 10)
        push, err := s.initPushContext(req, oldPushContext, versionLocal)
        if err != nil {
            return
        }

        initContextTime := time.Since(t0)
        // 输出Context初始化花费的时间
        adsLog.Debugf("InitContext %v for push took %s", versionLocal, initContextTime)

        versionMutex.Lock()
        version = versionLocal
        versionMutex.Unlock()

        req.Push = push
        s.AdsPushAll(versionLocal, req)
    }


    // AdsPushAll最终会去调用startPush
    // AdsPushAll implements old style invalidation, generated when any rule or endpoint changes.
    // Primary code path is from v1 discoveryService.clearCache(), which is added as a handler
    // to the model ConfigStorageCache and Controller.
    func (s *DiscoveryServer) AdsPushAll(version string, req *model.PushRequest) {
        // If we don't know what updated, cannot safely cache. Clear the whole cache
        if len(req.ConfigsUpdated) == 0 {
            s.Cache.ClearAll()
        } else {
            // Otherwise, just clear the updated configs
            s.Cache.Clear(req.ConfigsUpdated)
        }
        if !req.Full {
            adsLog.Infof("XDS: Incremental Pushing:%s ConnectedEndpoints:%d Version:%s",
                version, s.adsClientCount(), req.Push.PushVersion)
        } else {
            totalService := len(req.Push.Services(nil))
            adsLog.Infof("XDS: Pushing:%s Services:%d ConnectedEndpoints:%d  Version:%s",
                version, totalService, s.adsClientCount(), req.Push.PushVersion)
            monServices.Record(float64(totalService))

            // Make sure the ConfigsUpdated map exists
            if req.ConfigsUpdated == nil {
                req.ConfigsUpdated = make(map[model.ConfigKey]struct{})
            }
        }

        s.startPush(req)
    }

    // 最终将PushRequest放入到请求队列中
    // Send a signal to all connections, with a push event.
    func (s *DiscoveryServer) startPush(req *model.PushRequest) {
        // Push config changes, iterating over connected envoys. This cover ADS and EDS(0.7), both share
        // the same connection table

        if adsLog.DebugEnabled() {
            currentlyPending := s.pushQueue.Pending()
            if currentlyPending != 0 {
                adsLog.Infof("Starting new push while %v were still pending", currentlyPending)
            }
        }
        req.Start = time.Now()
        for _, p := range s.AllClients() {
            s.pushQueue.Enqueue(p, req)
        }
    }

    ```


// 最终doSendPushes会通过concurrentPushLimit来处理PushRequest
// 最多可以同时从Queue中获取到100个数据，然后通过协程构建PushEvent然后通过pushChannel，发给对应client的连接

```go

func (s *DiscoveryServer) sendPushes(stopCh <-chan struct{}) {
	doSendPushes(stopCh, s.concurrentPushLimit, s.pushQueue)
}

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
			client, push, shuttingdown := queue.Dequeue()
			if shuttingdown {
				return
			}
			recordPushTriggers(push.Reason...)
			// Signals that a push is done by reading from the semaphore, allowing another send on it.
			doneFunc := func() {
				queue.MarkDone(client)
				<-semaphore
			}

			proxiesQueueTime.Record(time.Since(push.Start).Seconds())

			go func() {
				// 组成中PushEvent
				pushEv := &Event{
					pushRequest: push,
					done:        doneFunc,
				}

				select {
				// 通过Connection中的pushChannel来让client处理。
				case client.pushChannel <- pushEv:
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

每次收到一个Stream就开启for循环，监听配置更新channel、和请求的Channel，一方面收到请求的时候需要Push
另外一方面收到配置更新的时候也需要进行Push。

```go
func (s *DiscoveryServer) Stream(stream DiscoveryStream) error {
	.....
	for {
		// Block until either a request is received or a push is triggered.
		// We need 2 go routines because 'read' blocks in Recv().
		//
		// To avoid 2 routines, we tried to have Recv() in StreamAggregateResource - and the push
		// on different short-lived go routines started when the push is happening. This would cut in 1/2
		// the number of long-running go routines, since push is throttled. The main problem is with
		// closing - the current gRPC library didn't allow closing the stream.
		select {
		case req, ok := <-reqChannel:
			if !ok {
				// Remote side closed connection or error processing the request.
				return receiveError
			}
			// processRequest is calling pushXXX, accessing common structs with pushConnection.
			// Adding sync is the second issue to be resolved if we want to save 1/2 of the threads.
			err := s.processRequest(req, con)
			if err != nil {
				return err
			}

		case pushEv := <-con.pushChannel:
			err := s.pushConnection(con, pushEv)
			pushEv.done()
			if err != nil {
				return err
			}
		case <-con.stop:
			return nil
		}
	}
}
```

// 最终调用pushConnection给一个client进行Push推送

```go
// Compute and send the new configuration for a connection. This is blocking and may be slow
// for large configs. The method will hold a lock on con.pushMutex.
func (s *DiscoveryServer) pushConnection(con *Connection, pushEv *Event) error {
	pushRequest := pushEv.pushRequest
	// 如果是Full Push，先需要更新ProxyState，主要是workload labels、service instance、sidecarscope、gateway等资源
	if pushRequest.Full {
		// Update Proxy with current information.
		s.updateProxy(con.proxy, pushRequest.Push)
	}

	// 接着判断这个Proxy是否需要Push
	if !s.ProxyNeedsPush(con.proxy, pushRequest) {
		adsLog.Debugf("Skipping push to %v, no updates required", con.ConID)
		if pushRequest.Full {
			// Only report for full versions, incremental pushes do not have a new version
			reportAllEvents(s.StatusReporter, con.ConID, pushRequest.Push.LedgerVersion, nil)
		}
		return nil
	}

	currentVersion := versionInfo()

	// Send pushes to all generators
	// Each Generator is responsible for determining if the push event requires a push
	for _, w := range getWatchedResources(con.proxy.WatchedResources) {
		if !features.EnableFlowControl {
			// Always send the push if flow control disabled
			if err := s.pushXds(con, pushRequest.Push, currentVersion, w, pushRequest); err != nil {
				return err
			}
			continue
		}
		// If flow control is enabled, we will only push if we got an ACK for the previous response
		synced, timeout := con.Synced(w.TypeUrl)
		if !synced && timeout {
			// We are not synced, but we have been stuck for too long. We will trigger the push anyways to
			// avoid any scenario where this may deadlock.
			// This can possibly be removed in the future if we find this never causes issues
			totalDelayedPushes.With(typeTag.Value(v3.GetMetricType(w.TypeUrl))).Increment()
			adsLog.Warnf("%s: QUEUE TIMEOUT for node:%s", v3.GetShortType(w.TypeUrl), con.proxy.ID)
		}
		if synced || timeout {
			// Send the push now
			if err := s.pushXds(con, pushRequest.Push, currentVersion, w, pushRequest); err != nil {
				return err
			}
		} else {
			// The type is not yet synced. Instead of pushing now, which may overload Envoy,
			// we will wait until the last push is ACKed and trigger the push. See
			// https://github.com/istio/istio/issues/25685 for details on the performance
			// impact of sending pushes before Envoy ACKs.
			totalDelayedPushes.With(typeTag.Value(v3.GetMetricType(w.TypeUrl))).Increment()
			adsLog.Debugf("%s: QUEUE for node:%s", v3.GetShortType(w.TypeUrl), con.proxy.ID)
			con.proxy.Lock()
			con.blockedPushes[w.TypeUrl] = con.blockedPushes[w.TypeUrl].Merge(pushEv.pushRequest)
			con.proxy.Unlock()
		}
	}
	if pushRequest.Full {
		// Report all events for unwatched resources. Watched resources will be reported in pushXds or on ack.
		reportAllEvents(s.StatusReporter, con.ConID, pushRequest.Push.LedgerVersion, con.proxy.WatchedResources)
	}

	proxiesConvergeDelay.Record(time.Since(pushRequest.Start).Seconds())
	return nil
}
```


如何确定一个配置变更是否需要推送给某个Proxy?

1. 首先无论是任何配置，在发生变更的时候都会带上GVK。

```go

    // 注册Service Handler的时候，也会带上ConfigsUpdated字段
	serviceHandler := func(svc *model.Service, _ model.Event) {
		pushReq := &model.PushRequest{
			Full: true,
			ConfigsUpdated: map[model.ConfigKey]struct{}{{
				Kind:      gvk.ServiceEntry,
				Name:      string(svc.Hostname),
				Namespace: svc.Attributes.Namespace,
			}: {}},
			Reason: []model.TriggerReason{model.ServiceUpdate},
		}
		s.XDSServer.ConfigUpdate(pushReq)
	}
	s.ServiceController().AppendServiceHandler(serviceHandler)


    // Config Controller注册配置变更Handler的时候会带上ConfigsUpdated字段，表明是什么配置、名字是什么、Namespace是什么等信息。
	if s.configController != nil {
		configHandler := func(old config.Config, curr config.Config, event model.Event) {
			pushReq := &model.PushRequest{
				Full: true,
				ConfigsUpdated: map[model.ConfigKey]struct{}{{
					Kind:      curr.GroupVersionKind,
					Name:      curr.Name,
					Namespace: curr.Namespace,
				}: {}},
				Reason: []model.TriggerReason{model.ConfigUpdate},
			}
			s.XDSServer.ConfigUpdate(pushReq)
			if event != model.EventDelete {
				s.statusReporter.AddInProgressResource(curr)
			} else {
				s.statusReporter.DeleteInProgressResource(curr)
			}
		}
        .....
```