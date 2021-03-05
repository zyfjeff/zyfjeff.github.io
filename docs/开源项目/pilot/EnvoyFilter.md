# EnvoyFilter分析

Envoy Filter和`Service`、`Virtual Service`、`DestinationRules`等等这些CRD资源都分为初始化、使用、更新等三个主要部分。

**初始化流程:**

1. Envoy首次连接到pilot后，调用`model.Environment`中的`PushContext`的`InitContext`方法初始化`PushContext`

    ```go
    // InitContext will initialize the data structures used for code generation.
    // This should be called before starting the push, from the thread creating
    // the push context.
    func (ps *PushContext) InitContext(env *Environment, oldPushContext *PushContext, pushReq *PushRequest) error {
        // .....
        // create new or incremental update
        if pushReq == nil || oldPushContext == nil || !oldPushContext.initDone.Load() || len(pushReq.ConfigsUpdated) == 0 {
            if err := ps.createNewContext(env); err != nil {
                return err
            }
        } else {
            if err := ps.updateContext(env, oldPushContext, pushReq); err != nil {
                return err
            }
        }
        // .....
    }
    ```

2. 通过`ps.createNewContext`初始化`Service`、V`irtualService`、`DestinationRules`、`Envoyfilters`等

    ```go
    func (ps *PushContext) createNewContext(env *Environment) error {
        ........
        if err := ps.initEnvoyFilters(env); err != nil {
            return err
        }	
        .......
    }
    ```

3. 通过`initEnvoyFilters`获取到所有的EnvoyFilter


4. 然后调用`func convertToEnvoyFilterWrapper(local *config.Config) *EnvoyFilterWrapper`，将EnvoyFilter转换成下面的`EnvoyFilterWrapper`

    一个EnvoyFilter的yaml最终会转换成下面这种形式。

    ```go
    // EnvoyFilterWrapper is a wrapper for the EnvoyFilter api object with pre-processed data
    type EnvoyFilterWrapper struct {
        workloadSelector labels.Instance
        Patches          map[networking.EnvoyFilter_ApplyTo][]*EnvoyFilterConfigPatchWrapper
    }

    // EnvoyFilterConfigPatchWrapper is a wrapper over the EnvoyFilter ConfigPatch api object
    // fields are ordered such that this struct is aligned
    type EnvoyFilterConfigPatchWrapper struct {
        Value     proto.Message
        Match     *networking.EnvoyFilter_EnvoyConfigObjectMatch
        ApplyTo   networking.EnvoyFilter_ApplyTo
        Operation networking.EnvoyFilter_Patch_Operation
        // Pre-compile the regex from proxy version match in the match
        ProxyVersionRegex *regexp.Regexp
        // ProxyPrefixMatch provides a prefix match for the proxy version. The current API only allows
        // regex match, but as an optimization we can reduce this to a prefix match for common cases.
        // If this is set, ProxyVersionRegex is ignored.
        ProxyPrefixMatch string
    }
    ```


**Apply流程:**

1. EnvoyFilter生效的地方主要有三个`BuildClusters`、`BuildExtensionConfiguration`、`patchListeners`

    首先来看下`BuildClusters`，分成了Gateway和Sidecar两种模式的Cluster生成，还会区分Inbound和Outbound Cluster

    ```go
    func (configgen *ConfigGeneratorImpl) BuildClusters(proxy *model.Proxy, push *model.PushContext) []*cluster.Cluster {
        clusters := make([]*cluster.Cluster, 0)
        envoyFilterPatches := push.EnvoyFilters(proxy)
        cb := NewClusterBuilder(proxy, push)
        instances := proxy.ServiceInstances

        switch proxy.Type {
        case model.SidecarProxy:
            // Setup outbound clusters
            outboundPatcher := clusterPatcher{envoyFilterPatches, networking.EnvoyFilter_SIDECAR_OUTBOUND}
            clusters = append(clusters, configgen.buildOutboundClusters(cb, outboundPatcher)...)
            // Add a blackhole and passthrough cluster for catching traffic to unresolved routes
            clusters = outboundPatcher.conditionallyAppend(clusters, cb.buildBlackHoleCluster(), cb.buildDefaultPassthroughCluster())
            clusters = append(clusters, outboundPatcher.insertedClusters()...)

            // Setup inbound clusters
            inboundPatcher := clusterPatcher{envoyFilterPatches, networking.EnvoyFilter_SIDECAR_INBOUND}
            clusters = append(clusters, configgen.buildInboundClusters(cb, instances, inboundPatcher)...)
            // Pass through clusters for inbound traffic. These cluster bind loopback-ish src address to access node local service.
            clusters = inboundPatcher.conditionallyAppend(clusters, cb.buildInboundPassthroughClusters()...)
            clusters = append(clusters, inboundPatcher.insertedClusters()...)
        default: // Gateways
            patcher := clusterPatcher{envoyFilterPatches, networking.EnvoyFilter_GATEWAY}
            clusters = append(clusters, configgen.buildOutboundClusters(cb, patcher)...)
            // Gateways do not require the default passthrough cluster as they do not have original dst listeners.
            clusters = patcher.conditionallyAppend(clusters, cb.buildBlackHoleCluster())
            if proxy.Type == model.Router && proxy.GetRouterMode() == model.SniDnatRouter {
                clusters = append(clusters, configgen.buildOutboundSniDnatClusters(proxy, push, patcher)...)
            }
            clusters = append(clusters, patcher.insertedClusters()...)
        }

        clusters = normalizeClusters(push, proxy, clusters)

        return clusters
    }

    func (p clusterPatcher) conditionallyAppend(l []*cluster.Cluster, clusters ...*cluster.Cluster) []*cluster.Cluster {
        for _, c := range clusters {
            if envoyfilter.ShouldKeepCluster(p.pctx, p.efw, c) {
                l = append(l, envoyfilter.ApplyClusterMerge(p.pctx, p.efw, c))
            }
        }
        return l
    }

    func (p clusterPatcher) insertedClusters() []*cluster.Cluster {
        return envoyfilter.InsertedClusters(p.pctx, p.efw)
    }
    ```

    先生成Cluster、生成完Cluster后，获取到EnvoyFilter，然后调用EnovyFilter`CLuster Patch`，`Cluster Patch`核心需要关心三个操作，Remove、Patch、Add三类


    ShouldKeepCluster就是用来决定一个Cluster是否需要删除的操作。

    ```go
    func ShouldKeepCluster(pctx networking.EnvoyFilter_PatchContext, efw *model.EnvoyFilterWrapper, c *cluster.Cluster) bool {
        if efw == nil {
            return true
        }
        for _, cp := range efw.Patches[networking.EnvoyFilter_CLUSTER] {
            if cp.Operation != networking.EnvoyFilter_Patch_REMOVE {
                continue
            }
            // commonConditionMatch用来匹配PatchContext，
            if commonConditionMatch(pctx, cp) && clusterMatch(c, cp) {
                return false
            }
        }
        return true
    }
    ```

    ApplyClusterMerge则是用来将EnvoyFilter中的Cluster相关的内容进行Merge

    ```go
    func ApplyClusterMerge(pctx networking.EnvoyFilter_PatchContext, efw *model.EnvoyFilterWrapper, c *cluster.Cluster) (out *cluster.Cluster) {
        defer runtime.HandleCrash(runtime.LogPanic, func(interface{}) {
            log.Errorf("clusters patch caused panic, so the patches did not take effect")
        })
        // In case the patches cause panic, use the clusters generated before to reduce the influence.
        out = c
        if efw == nil {
            return
        }
        for _, cp := range efw.Patches[networking.EnvoyFilter_CLUSTER] {
            if cp.Operation != networking.EnvoyFilter_Patch_MERGE {
                continue
            }
            if commonConditionMatch(pctx, cp) && clusterMatch(c, cp) {
                proto.Merge(c, cp.Value)
            }
        }
        return c
    }
    ```

    最后一类就是把新增的Cluster插入

    ```go
    func InsertedClusters(pctx networking.EnvoyFilter_PatchContext, efw *model.EnvoyFilterWrapper) []*cluster.Cluster {
        if efw == nil {
            return nil
        }
        var result []*cluster.Cluster
        // Add cluster if the operation is add, and patch context matches
        for _, cp := range efw.Patches[networking.EnvoyFilter_CLUSTER] {
            if cp.Operation == networking.EnvoyFilter_Patch_ADD {
                if commonConditionMatch(pctx, cp) {
                    result = append(result, proto.Clone(cp.Value).(*cluster.Cluster))
                }
            }
        }
        return result
    }
    ```

    总的来说，Cluster主要有Merge、Add、Remove三个操作，通过`ShouldKeepCluster`来决定Cluster是否跳过，通过`InsertedClusters`来完成Add的所有Cluster，通过
    `ApplyClusterMerge`来完成Merge相关资源的合并。


    接着来看下`patchListeners`，先通过BuildListener构建好完成的Listener，然后调用patchListener进行EnvoyFilter的生效

    ```go
    // BuildListeners produces a list of listeners and referenced clusters for all proxies
    func (configgen *ConfigGeneratorImpl) BuildListeners(node *model.Proxy,
        push *model.PushContext) []*listener.Listener {
        builder := NewListenerBuilder(node, push)

        switch node.Type {
        case model.SidecarProxy:
            builder = configgen.buildSidecarListeners(builder)
        case model.Router:
            builder = configgen.buildGatewayListeners(builder)
        }

        builder.patchListeners()
        return builder.getListeners()
    }
    ```

    `ptachListeners`，首先区分Sidrcar和Router两种模式分开做Apply，接着针对VirtualInbound、VirtualOutbound、Inbound、Outbound四种类型进行Apply。
    最终都用去调用`envoyfilter.ApplyListenerPatches`。

    ```go
    func (lb *ListenerBuilder) patchListeners() {
        lb.envoyFilterWrapper = lb.push.EnvoyFilters(lb.node)
        if lb.envoyFilterWrapper == nil {
            return
        }

        if lb.node.Type == model.Router {
            lb.gatewayListeners = envoyfilter.ApplyListenerPatches(networking.EnvoyFilter_GATEWAY, lb.node, lb.push, lb.envoyFilterWrapper,
                lb.gatewayListeners, false)
            return
        }
        // virtual outbound只是一个Listenr，所以这里只是调用patchOneListener
        lb.virtualOutboundListener = lb.patchOneListener(lb.virtualOutboundListener, networking.EnvoyFilter_SIDECAR_OUTBOUND)
        lb.virtualInboundListener = lb.patchOneListener(lb.virtualInboundListener, networking.EnvoyFilter_SIDECAR_INBOUND)
        lb.inboundListeners = envoyfilter.ApplyListenerPatches(networking.EnvoyFilter_SIDECAR_INBOUND, lb.node,
            lb.push, lb.envoyFilterWrapper, lb.inboundListeners, false)
        lb.outboundListeners = envoyfilter.ApplyListenerPatches(networking.EnvoyFilter_SIDECAR_OUTBOUND, lb.node,
            lb.push, lb.envoyFilterWrapper, lb.outboundListeners, false)
    }

    // 适配ApplyListenerPatches这个接口，因为这个接口接收的是list
    func (lb *ListenerBuilder) patchOneListener(l *listener.Listener, ctx networking.EnvoyFilter_PatchContext) *listener.Listener {
        if l == nil {
            return nil
        }
        tempArray := []*listener.Listener{l}
        tempArray = envoyfilter.ApplyListenerPatches(ctx, lb.node, lb.push, lb.envoyFilterWrapper, tempArray, true)
        // temp array will either be empty [if virtual listener was removed] or will have a modified listener
        if len(tempArray) == 0 {
            return nil
        }
        return tempArray[0]
    }
    ```

    最后来分析下`envoyfilter.ApplyListenerPatches`即可。

    ```go
    // ApplyListenerPatches applies patches to LDS output
    func ApplyListenerPatches(
        patchContext networking.EnvoyFilter_PatchContext,
        proxy *model.Proxy,
        push *model.PushContext,
        efw *model.EnvoyFilterWrapper,
        listeners []*xdslistener.Listener,
        skipAdds bool) (out []*xdslistener.Listener) {
        defer runtime.HandleCrash(runtime.LogPanic, func(interface{}) {
            log.Errorf("listeners patch caused panic, so the patches did not take effect")
        })
        // In case the patches cause panic, use the listeners generated before to reduce the influence.
        out = listeners

        if efw == nil {
            return
        }

        return doListenerListOperation(patchContext, efw, listeners, skipAdds)
    }

    func doListenerListOperation(
        patchContext networking.EnvoyFilter_PatchContext,
        envoyFilterWrapper *model.EnvoyFilterWrapper,
        listeners []*xdslistener.Listener,
        skipAdds bool) []*xdslistener.Listener {
        listenersRemoved := false

        // do all the changes for a single envoy filter crd object. [including adds]
        // then move on to the next one

        // only removes/merges plus next level object operations [add/remove/merge]
        // 排除Name为空的listener,所有要移除的listener都会把name设置为""
        for _, listener := range listeners {
            if listener.Name == "" {
                // removed by another op
                continue
            }
            doListenerOperation(patchContext, envoyFilterWrapper.Patches, listener, &listenersRemoved)
        }
        // adds at listener level if enabled
        // 处理Add
        if !skipAdds {
            for _, cp := range envoyFilterWrapper.Patches[networking.EnvoyFilter_LISTENER] {
                if cp.Operation == networking.EnvoyFilter_Patch_ADD {
                    if !commonConditionMatch(patchContext, cp) {
                        continue
                    }

                    // clone before append. Otherwise, subsequent operations on this listener will corrupt
                    // the master value stored in CP..
                    listeners = append(listeners, proto.Clone(cp.Value).(*xdslistener.Listener))
                }
            }
        }
        // 处理remove，在doListenerOperation中已经把要移除的listener的name设置为空了
        if listenersRemoved {
            tempArray := make([]*xdslistener.Listener, 0, len(listeners))
            for _, l := range listeners {
                if l.Name != "" {
                    tempArray = append(tempArray, l)
                }
            }
            return tempArray
        }
        return listeners
    }
    ```

    总结来说，处理listener的时候先处理remove，需要remove的先标记name为空，然后处理listener的merge操作，最后处理filter chain的合并、network filter chain等，思路和listener的处理基本类似。
    处理完这些后开始处理listener的add、最后把之前标记要删除的lisetner都删除即可。


**更新流程:**

1. EnvoyFilter发生变更
2. 