

# PushContext分析

1. 收到xds请求的时候，会初始化全局Context，只会初始化一次，存放在`DiscoveryServer`中的`model.Environment`中

2. 收到Push请求的时候，会触发PushContext的更新和创建

    ```go
    func (s *DiscoveryServer) Push(req *model.PushRequest) {
        if !req.Full {
            // 不是全量的化就用global push context
            req.Push = s.globalPushContext()
            s.AdsPushAll(versionInfo(), req)
            return
        }
        // Reset the status during the push.
        oldPushContext := s.globalPushContext()
        if oldPushContext != nil {
            oldPushContext.OnConfigChange()
        }
        // PushContext is reset after a config change. Previous status is
        // saved.
        t0 := time.Now()

        versionLocal := time.Now().Format(time.RFC3339) + "/" + strconv.FormatUint(versionNum.Inc(), 10)
        // 如果是全量就重新创建新的PushContext
        push, err := s.initPushContext(req, oldPushContext, versionLocal)
        if err != nil {
            return
        }

        initContextTime := time.Since(t0)
        adsLog.Debugf("InitContext %v for push took %s", versionLocal, initContextTime)

        versionMutex.Lock()
        version = versionLocal
        versionMutex.Unlock()

        req.Push = push
        s.AdsPushAll(versionLocal, req)
    }
    ```