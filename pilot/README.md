

* IstioConfigStore

```go
// ConfigStore describes a set of platform agnostic APIs that must be supported
// by the underlying platform to store and retrieve Istio configuration.
//
// Configuration key is defined to be a combination of the type, name, and
// namespace of the configuration object. The configuration key is guaranteed
// to be unique in the store.
//
// The storage interface presented here assumes that the underlying storage
// layer supports _Get_ (list), _Update_ (update), _Create_ (create) and
// _Delete_ semantics but does not guarantee any transactional semantics.
//
// _Update_, _Create_, and _Delete_ are mutator operations. These operations
// are asynchronous, and you might not see the effect immediately (e.g. _Get_
// might not return the object by key immediately after you mutate the store.)
// Intermittent errors might occur even though the operation succeeds, so you
// should always check if the object store has been modified even if the
// mutating operation returns an error.  Objects should be created with
// _Create_ operation and updated with _Update_ operation.
//
// Resource versions record the last mutation operation on each object. If a
// mutation is applied to a different revision of an object than what the
// underlying storage expects as defined by pure equality, the operation is
// blocked.  The client of this interface should not make assumptions about the
// structure or ordering of the revision identifier.
//
// Object references supplied and returned from this interface should be
// treated as read-only. Modifying them violates thread-safety.
type ConfigStore interface {
	// Schemas exposes the configuration type schema known by the config store.
	// The type schema defines the bidrectional mapping between configuration
	// types and the protobuf encoding schema.
	Schemas() collection.Schemas

	// Get retrieves a configuration element by a type and a key
	Get(typ config.GroupVersionKind, name, namespace string) *config.Config

	// List returns objects by type and namespace.
	// Use "" for the namespace to list across namespaces.
	List(typ config.GroupVersionKind, namespace string) ([]config.Config, error)

	// Create adds a new configuration object to the store. If an object with the
	// same name and namespace for the type already exists, the operation fails
	// with no side effects.
	Create(config config.Config) (revision string, err error)

	// Update modifies an existing configuration object in the store.  Update
	// requires that the object has been created.  Resource version prevents
	// overriding a value that has been changed between prior _Get_ and _Put_
	// operation to achieve optimistic concurrency. This method returns a new
	// revision if the operation succeeds.
	Update(config config.Config) (newRevision string, err error)

	UpdateStatus(config config.Config) (newRevision string, err error)

	// Patch applies only the modifications made in the PatchFunc rather than doing a full replace. Useful to avoid
	// read-modify-write conflicts when there are many concurrent-writers to the same resource.
	Patch(orig config.Config, patchFn config.PatchFunc) (string, error)

	// Delete removes an object from the store by key
	// For k8s, resourceVersion must be fulfilled before a deletion is carried out.
	// If not possible, a 409 Conflict status will be returned.
	Delete(typ config.GroupVersionKind, name, namespace string, resourceVersion *string) error
}

// IstioConfigStore is a specialized interface to access config store using
// Istio configuration types
// nolint
type IstioConfigStore interface {
	ConfigStore

	// ServiceEntries lists all service entries
	ServiceEntries() []config.Config

	// Gateways lists all gateways bound to the specified workload labels
	Gateways(workloadLabels labels.Collection) []config.Config

	// AuthorizationPolicies selects AuthorizationPolicies in the specified namespace.
	AuthorizationPolicies(namespace string) []config.Config
}
```

* `ServiceDiscovery`

```go
// ServiceDiscovery enumerates Istio service instances.
// nolint: lll
type ServiceDiscovery interface {
	// Services list declarations of all services in the system
	Services() ([]*Service, error)

	// GetService retrieves a service by host name if it exists
	GetService(hostname host.Name) (*Service, error)

	// InstancesByPort retrieves instances for a service on the given ports with labels that match
	// any of the supplied labels. All instances match an empty tag list.
	//
	// For example, consider an example of catalog.mystore.com:
	// Instances(catalog.myservice.com, 80) ->
	//      --> IstioEndpoint(172.16.0.1:8888), Service(catalog.myservice.com), Labels(foo=bar)
	//      --> IstioEndpoint(172.16.0.2:8888), Service(catalog.myservice.com), Labels(foo=bar)
	//      --> IstioEndpoint(172.16.0.3:8888), Service(catalog.myservice.com), Labels(kitty=cat)
	//      --> IstioEndpoint(172.16.0.4:8888), Service(catalog.myservice.com), Labels(kitty=cat)
	//
	// Calling Instances with specific labels returns a trimmed list.
	// e.g., Instances(catalog.myservice.com, 80, foo=bar) ->
	//      --> IstioEndpoint(172.16.0.1:8888), Service(catalog.myservice.com), Labels(foo=bar)
	//      --> IstioEndpoint(172.16.0.2:8888), Service(catalog.myservice.com), Labels(foo=bar)
	//
	// Similar concepts apply for calling this function with a specific
	// port, hostname and labels.
	//
	// Introduced in Istio 0.8. It is only called with 1 port.
	// CDS (clusters.go) calls it for building 'dnslb' type clusters.
	// EDS calls it for building the endpoints result.
	// Consult istio-dev before using this for anything else (except debugging/tools)
	InstancesByPort(svc *Service, servicePort int, labels labels.Collection) []*ServiceInstance

	// GetProxyServiceInstances returns the service instances that co-located with a given Proxy
	//
	// Co-located generally means running in the same network namespace and security context.
	//
	// A Proxy operating as a Sidecar will return a non-empty slice.  A stand-alone Proxy
	// will return an empty slice.
	//
	// There are two reasons why this returns multiple ServiceInstances instead of one:
	// - A ServiceInstance has a single IstioEndpoint which has a single Port.  But a Service
	//   may have many ports.  So a workload implementing such a Service would need
	//   multiple ServiceInstances, one for each port.
	// - A single workload may implement multiple logical Services.
	//
	// In the second case, multiple services may be implemented by the same physical port number,
	// though with a different ServicePort and IstioEndpoint for each.  If any of these overlapping
	// services are not HTTP or H2-based, behavior is undefined, since the listener may not be able to
	// determine the intended destination of a connection without a Host header on the request.
	GetProxyServiceInstances(*Proxy) []*ServiceInstance

	GetProxyWorkloadLabels(*Proxy) labels.Collection

	// GetIstioServiceAccounts returns a list of service accounts looked up from
	// the specified service hostname and ports.
	// Deprecated - service account tracking moved to XdsServer, incremental.
	GetIstioServiceAccounts(svc *Service, ports []int) []string

	// NetworkGateways returns a map of network name to Gateways that can be used to access that network.
	NetworkGateways() map[string][]*Gateway
}

```

* `Enviroment` API的合集，包含了服务发现、配置存储、Mesh Config、Mesh Network Config的文件Waitch、PushContext等 

```go
// Environment provides an aggregate environmental API for Pilot
type Environment struct {
	// Discovery interface for listing services and instances.
	ServiceDiscovery

	// Config interface for listing routing rules
	IstioConfigStore

	// Watcher is the watcher for the mesh config (to be merged into the config store)
	mesh.Watcher

	// NetworksWatcher (loaded from a config map) provides information about the
	// set of networks inside a mesh and how to route to endpoints in each
	// network. Each network provides information about the endpoints in a
	// routable L3 network. A single routable L3 network can have one or more
	// service registries.
	mesh.NetworksWatcher

	// PushContext holds informations during push generation. It is reset on config change, at the beginning
	// of the pushAll. It will hold all errors and stats and possibly caches needed during the entire cache computation.
	// DO NOT USE EXCEPT FOR TESTS AND HANDLING OF NEW CONNECTIONS.
	// ALL USE DURING A PUSH SHOULD USE THE ONE CREATED AT THE
	// START OF THE PUSH, THE GLOBAL ONE MAY CHANGE AND REFLECT A DIFFERENT
	// CONFIG AND PUSH
	PushContext *PushContext

	// DomainSuffix provides a default domain for the Istio server.
	DomainSuffix string

	ledger ledger.Ledger
}
```

* `DiscoveryServer`

```go
// DiscoveryServer is Pilot's gRPC implementation for Envoy's xds APIs
type DiscoveryServer struct {
	// Env is the model environment.
	Env *model.Environment

	// MemRegistry is used for debug and load testing, allow adding services. Visible for testing.
	MemRegistry *memory.ServiceDiscovery

	// ConfigGenerator is responsible for generating data plane configuration using Istio networking
	// APIs and service registry info
	ConfigGenerator core.ConfigGenerator

	// Generators allow customizing the generated config, based on the client metadata.
	// Key is the generator type - will match the Generator metadata to set the per-connection
	// default generator, or the combination of Generator metadata and TypeUrl to select a
	// different generator for a type.
	// Normal istio clients use the default generator - will not be impacted by this.
	Generators map[string]model.XdsResourceGenerator

	concurrentPushLimit chan struct{}
	// mutex protecting global structs updated or read by ADS service, including ConfigsUpdated and
	// shards.
	mutex sync.RWMutex

	// InboundUpdates describes the number of configuration updates the discovery server has received
	InboundUpdates *atomic.Int64
	// CommittedUpdates describes the number of configuration updates the discovery server has
	// received, process, and stored in the push context. If this number is less than InboundUpdates,
	// there are updates we have not yet processed.
	// Note: This does not mean that all proxies have received these configurations; it is strictly
	// the push context, which means that the next push to a proxy will receive this configuration.
	CommittedUpdates *atomic.Int64

	// EndpointShards for a service. This is a global (per-server) list, built from
	// incremental updates. This is keyed by service and namespace
	EndpointShardsByService map[string]map[string]*EndpointShards

	pushChannel chan *model.PushRequest

	// mutex used for config update scheduling (former cache update mutex)
	updateMutex sync.RWMutex

	// pushQueue is the buffer that used after debounce and before the real xds push.
	pushQueue *PushQueue

	// debugHandlers is the list of all the supported debug handlers.
	debugHandlers map[string]string

	// adsClients reflect active gRPC channels, for both ADS and EDS.
	adsClients      map[string]*Connection
	adsClientsMutex sync.RWMutex

	StatusReporter DistributionStatusCache

	// Authenticators for XDS requests. Should be same/subset of the CA authenticators.
	Authenticators []authenticate.Authenticator

	// StatusGen is notified of connect/disconnect/nack on all connections
	StatusGen               *StatusGen
	WorkloadEntryController *workloadentry.Controller

	// serverReady indicates caches have been synced up and server is ready to process requests.
	serverReady atomic.Bool

	debounceOptions debounceOptions

	instanceID string

	// Cache for XDS resources
	Cache model.XdsCache
}
```

* `PushContext`

```go
// PushContext tracks the status of a push - metrics and errors.
// Metrics are reset after a push - at the beginning all
// values are zero, and when push completes the status is reset.
// The struct is exposed in a debug endpoint - fields public to allow
// easy serialization as json.
type PushContext struct {
	proxyStatusMutex sync.RWMutex
	// ProxyStatus is keyed by the error code, and holds a map keyed
	// by the ID.
	ProxyStatus map[string]map[string]ProxyPushStatus

	// Synthesized from env.Mesh
	exportToDefaults exportToDefaults

	// ServiceIndex is the index of services by various fields.
	ServiceIndex serviceIndex

	// ServiceAccounts contains a map of hostname and port to service accounts.
	ServiceAccounts map[host.Name]map[int][]string `json:"-"`

	// virtualServiceIndex is the index of virtual services by various fields.
	virtualServiceIndex virtualServiceIndex

	// destinationRuleIndex is the index of destination rules by various fields.
	destinationRuleIndex destinationRuleIndex

	// gatewayIndex is the index of gateways.
	gatewayIndex gatewayIndex

	// clusterLocalHosts extracted from the MeshConfig
	clusterLocalHosts host.Names

	// sidecars for each namespace
	sidecarsByNamespace map[string][]*SidecarScope

	// envoy filters for each namespace including global config namespace
	envoyFiltersByNamespace map[string][]*EnvoyFilterWrapper

	// The following data is either a global index or used in the inbound path.
	// Namespace specific views do not apply here.

	// AuthzPolicies stores the existing authorization policies in the cluster. Could be nil if there
	// are no authorization policies in the cluster.
	AuthzPolicies *AuthorizationPolicies `json:"-"`

	// Mesh configuration for the mesh.
	Mesh *meshconfig.MeshConfig `json:"-"`

	// Networks configuration.
	MeshNetworks *meshconfig.MeshNetworks `json:"-"`

	// Discovery interface for listing services and instances.
	ServiceDiscovery `json:"-"`

	// Config interface for listing routing rules
	IstioConfigStore `json:"-"`

	// AuthnBetaPolicies contains (beta) Authn policies by namespace.
	AuthnBetaPolicies *AuthenticationPolicies `json:"-"`

	Version string

	// cache gateways addresses for each network
	// this is mainly used for kubernetes multi-cluster scenario
	networkGateways map[string][]*Gateway

	initDone        atomic.Bool
	initializeMutex sync.Mutex
}
```



## 基本组件和概念

* `labels.Instance` 用于实现label匹配的，典型的实现场景在于`WorkloadSelector`，这是一个map，里面可以填一些key、value，用于匹配符合条件的节点。
* `networking.EnvoyFilter_PatchContext` Inbound、Outbound、Sidecar、Gateway、Any，也就是EnvoyFilter生效的范围。
* `WatchedNamespaces` 默认pilot从所有的namespace中获取服务信息，如果配置了这个参数则只会从指定的namespace中获取服务信息
* `RootNamespace` istio配置的admin namespace，放在这里的配置全局生效
* `ConfigNamespace` Proxy所在的Namespace，由Proxy带上去的。


## Envoy Filter

Envoy Filter和`Service`、`Virtual Service`、`DestinationRules`等等这些CRD资源都分为初始化、使用、更新等三个主要部分。

初始化流程:

1. Envoy首次连接到pilot后，调用model.Environment中的PushContext的InitContext方法初始化PushContext

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

2. 通过`ps.createNewContext`初始化Service、VirtualService、DestinationRules、Envoyfilters等

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

Apply流程:

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

先生成Cluster、生成完Cluster后，获取到EnvoyFilter，然后调用EnovyFilter`CLuster Patch`，Cluster Patch核心需要关心三个操作，Remove、Patch、Add三类


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


更新流程:

1. EnvoyFilter发生变更
2. 

## PushContext

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


## 配置更新流程

1. MeshConfig、 MeshNetworkConfig 配置发生变更会触发Full Push

```go
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

```go
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
			if old.Generation == curr.Generation && curr.Generation != 0 {
				// Kubernetes will start generation at 1, but some internally generated configurations
				// may not set resource version at all and we still want updates from these
				if curr.GroupVersionKind == gvk.WorkloadEntry {
					oldCond := modelstatus.GetConditionFromSpec(old, modelstatus.ConditionHealthy)
					newCond := modelstatus.GetConditionFromSpec(curr, modelstatus.ConditionHealthy)
					if oldCond == newCond {
						return
					}
				} else if onlyStatusUpdated(old, curr) {
					log.Debugf("skipping push for %v/%v, due to no change in spec or labels\n",
						old.Namespace, old.Name)
					return
				}
			}

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
		schemas := collections.Pilot.All()
		if features.EnableServiceApis {
			schemas = collections.PilotServiceApi.All()
		}
		for _, schema := range schemas {
			// This resource type was handled in external/servicediscovery.go, no need to rehandle here.
			if schema.Resource().GroupVersionKind() == collections.IstioNetworkingV1Alpha3Serviceentries.
				Resource().GroupVersionKind() {
				continue
			}
			if schema.Resource().GroupVersionKind() == collections.IstioNetworkingV1Alpha3Workloadentries.
				Resource().GroupVersionKind() {
				continue
			}

			s.configController.RegisterEventHandler(schema.Resource().GroupVersionKind(), configHandler)
		}
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
## PushRequest


## ServiceDiscovery


* `Services() ([]*Service, error)` 获取所有的Service
* `GetService(hostname host.Name) (*Service, error)` 根据host Name来获取指定Service
* `InstancesByPort(svc *Service, servicePort int, labels labels.Collection) []*ServiceInstance` 基于service port和label进行查找指定Service的instance
* `GetProxyServiceInstances(*Proxy) []*ServiceInstances`
* `GetProxyWorkloadLabels(*Proxy) labels.Collection` 返回指定proxy所属于的ServiceInstance

> ServiceInstance 是service + service port + 实际服务的端口 + ip + labels 组合而来
> 如果一个proxy有多个service，那么GetProxyServiceInstances返回多个ServiceInstance


## 服务可见性

通过`networking.istio.io/exportTo`这个Annotations可以指定这个service是否暴露给其他namespace的sidecar访问，
可选的值有`*`(暴露给所有的namespace)、`.`(只在当前namespace)、`~`(不暴露给任何，仅自己访问)。

```go
	if svc.Annotations[annotation.NetworkingExportTo.Name] != "" {
		exportTo = make(map[visibility.Instance]bool)
		for _, e := range strings.Split(svc.Annotations[annotation.NetworkingExportTo.Name], ",") {
			exportTo[visibility.Instance(e)] = true
		}
	}
```

最终将exportTo保存在服务的`Attributes`中。

## Sidecar

1. Sidecar通过workloadSelector来选择生效的Proxy，如果没有配置就对所有的Proxy生效，一个Namespace只能有一个没有workloadSelector的Sidecar
2. 优先选择带有workloadSelector的Sidecar
3. 放在Root Namespace下的sidecar对所有Namespace下没有Sidecar配置Namespace生效，这个全局的默认Sidecar配置是没有workloadSelector

```yaml
apiVersion: networking.istio.io/v1beta1
kind: Sidecar
metadata:
  name: default
  namespace: istio-config
spec:
  egress:
  - hosts:
	# 可以访问当前namesapce下所有的服务
	- "./*"
	# 可以访问istio-system下所有的服务
    - "istio-system/*"
```


```yaml
apiVersion: networking.istio.io/v1beta1
kind: Sidecar 
metadata:
  name: ratings
  namespace: prod-us1
spec:
  workloadSelector:
    labels:
      app: ratings
  ingress:
  - port:
      number: 9080
      protocol: HTTP
      name: somename
    defaultEndpoint: unix:///var/run/someuds.sock
  egress:
  - port:
      number: 9080
      protocol: HTTP
      name: egresshttp
    hosts:
    - "prod-us1/*"
  - hosts:
    - "istio-system/*"
```

SidecarScopes的初始化

```go
func (ps *PushContext) initSidecarScopes(env *Environment) error {
	sidecarConfigs, err := env.List(gvk.Sidecar, NamespaceAll)
	if err != nil {
		return err
	}

	sortConfigByCreationTime(sidecarConfigs)

	sidecarConfigWithSelector := make([]config.Config, 0)
	sidecarConfigWithoutSelector := make([]config.Config, 0)
	sidecarsWithoutSelectorByNamespace := make(map[string]struct{})
	for _, sidecarConfig := range sidecarConfigs {
		sidecar := sidecarConfig.Spec.(*networking.Sidecar)
		if sidecar.WorkloadSelector != nil {
			sidecarConfigWithSelector = append(sidecarConfigWithSelector, sidecarConfig)
		} else {
			sidecarsWithoutSelectorByNamespace[sidecarConfig.Namespace] = struct{}{}
			sidecarConfigWithoutSelector = append(sidecarConfigWithoutSelector, sidecarConfig)
		}
	}

	sidecarNum := len(sidecarConfigs)
	sidecarConfigs = make([]config.Config, 0, sidecarNum)
	sidecarConfigs = append(sidecarConfigs, sidecarConfigWithSelector...)
	sidecarConfigs = append(sidecarConfigs, sidecarConfigWithoutSelector...)

	ps.sidecarsByNamespace = make(map[string][]*SidecarScope, sidecarNum)
	for _, sidecarConfig := range sidecarConfigs {
		sidecarConfig := sidecarConfig
		ps.sidecarsByNamespace[sidecarConfig.Namespace] = append(ps.sidecarsByNamespace[sidecarConfig.Namespace],
			ConvertToSidecarScope(ps, &sidecarConfig, sidecarConfig.Namespace))
	}

	// Hold reference root namespace's sidecar config
	// Root namespace can have only one sidecar config object
	// Currently we expect that it has no workloadSelectors
	var rootNSConfig *config.Config
	if ps.Mesh.RootNamespace != "" {
		for _, sidecarConfig := range sidecarConfigs {
			if sidecarConfig.Namespace == ps.Mesh.RootNamespace &&
				sidecarConfig.Spec.(*networking.Sidecar).WorkloadSelector == nil {
				rootNSConfig = &sidecarConfig
				break
			}
		}
	}

	// build sidecar scopes for namespaces that do not have a non-workloadSelector sidecar CRD object.
	// Derive the sidecar scope from the root namespace's sidecar object if present. Else fallback
	// to the default Istio behavior mimicked by the DefaultSidecarScopeForNamespace function.
	namespaces := sets.NewSet()
	for _, nsMap := range ps.ServiceIndex.HostnameAndNamespace {
		for ns := range nsMap {
			namespaces.Insert(ns)
		}
	}
	for ns := range namespaces {
		if _, exist := sidecarsWithoutSelectorByNamespace[ns]; !exist {
			ps.sidecarsByNamespace[ns] = append(ps.sidecarsByNamespace[ns], ConvertToSidecarScope(ps, rootNSConfig, ns))
		}
	}

	return nil
}
```

当Proxy连接上来的时候会给其生成Sidecar

```go
func (node *Proxy) SetSidecarScope(ps *PushContext) {
	sidecarScope := node.SidecarScope

	if node.Type == SidecarProxy {
		workloadLabels := labels.Collection{node.Metadata.Labels}
		node.SidecarScope = ps.getSidecarScope(node, workloadLabels)
	} else {
		// Gateways should just have a default scope with egress: */*
		node.SidecarScope = DefaultSidecarScopeForNamespace(ps, node.ConfigNamespace)
	}
	node.PrevSidecarScope = sidecarScope
}
```

## Debounce

1. 配置发生变更的时候，通过注册的handler，调用ConfigUpdate

```go
func (s *DiscoveryServer) ConfigUpdate(req *model.PushRequest) {
	inboundConfigUpdates.Increment()
	s.InboundUpdates.Inc()
	s.pushChannel <- req
}
```

2. 通过PushChannel收到Push请求，开始做debounce

核心的几个chan分别是timeChan、用来触发一个时间间隔，叫做debounceAfter，一次推送至少需要间隔debounceAfter的时间，才会继续触发下一次推送。在
此期间出现的Push请求都会被合并。freeCh是用来做连续推送的，如果一次推送耗时太久超过了debounceAfter就需要通过freeCh在推送完成后继续推送。

```go
// The debounce helper function is implemented to enable mocking
func debounce(ch chan *model.PushRequest, stopCh <-chan struct{}, opts debounceOptions, pushFn func(req *model.PushRequest), updateSent *atomic.Int64) {
	var timeChan <-chan time.Time
	var startDebounce time.Time
	var lastConfigUpdateTime time.Time

	pushCounter := 0
	debouncedEvents := 0

	// Keeps track of the push requests. If updates are debounce they will be merged.
	var req *model.PushRequest

	free := true
	freeCh := make(chan struct{}, 1)

	push := func(req *model.PushRequest, debouncedEvents int) {
		pushFn(req)
		updateSent.Add(int64(debouncedEvents))
		freeCh <- struct{}{}
	}

	pushWorker := func() {
		// 3. 计算从开始debounce到推送的时间间隔，以及上一次配置变更到现在的安静时间
		eventDelay := time.Since(startDebounce)
		quietTime := time.Since(lastConfigUpdateTime)
		// it has been too long or quiet enough
		// 4. 如果从debounce到推送的间隔大于debounceMax就触发推送，或者在debounceAfter期间没有任何配置变更，也会触发推送
		if eventDelay >= opts.debounceMax || quietTime >= opts.debounceAfter {
			if req != nil {
				pushCounter++
				adsLog.Infof("Push debounce stable[%d] %d: %v since last change, %v since last push, full=%v",
					pushCounter, debouncedEvents,
					quietTime, eventDelay, req.Full)
				// 5. 推送的时候设置free = false，避免此时配置有变更，再次触发推送，然后重新设置debouncedEvents，触发下一次debounce
				free = false
				// 6. 推送完成后会重新设置free等于true，于此同时还会再次触发推送，避免因为上一次推送太耗时，导致推送堆积，因此需要第一时间再次进行推送
				go push(req, debouncedEvents)
				req = nil
				debouncedEvents = 0
			}
		} else {
			timeChan = time.After(opts.debounceAfter - quietTime)
		}
	}

	for {
		select {
		case <-freeCh:
			free = true
			pushWorker()
		case r := <-ch:
			// 收到推送请求，开始处理，对于EDS则直接Push，跳过debounce
			// 对于非EDS的配置开始执行debounce
			// If reason is not set, record it as an unknown reason
			if len(r.Reason) == 0 {
				r.Reason = []model.TriggerReason{model.UnknownTrigger}
			}
			if !opts.enableEDSDebounce && !r.Full {
				// trigger push now, just for EDS
				go pushFn(r)
				continue
			}
			// 1. 设置debounch timer
			lastConfigUpdateTime = time.Now()
			if debouncedEvents == 0 {
				timeChan = time.After(opts.debounceAfter)
				startDebounce = lastConfigUpdateTime
			}
			debouncedEvents++

			req = req.Merge(r)
		case <-timeChan:
			// 2. Debounc时间到了后，开始执行PushWorker
			if free {
				pushWorker()
			}
		case <-stopCh:
			return
		}
	}
}
```
> 总结来看，debounceMax才是控制推送的最大间隔，而debounceAfter则控制了推送的最小间隔。
> debounceAfter只有在一个完整期间没有任何配置变更的时候，才会触发推送。而debounceMax则是累计值
> 累计超过debounceMax会强制触发一次推送。



## ProxyNeedsPush

```golang
// DefaultProxyNeedsPush check if a proxy needs push for this push event.
func DefaultProxyNeedsPush(proxy *model.Proxy, req *model.PushRequest) bool {
	// 先测试这个配置是否影响这个Proxy
	if ConfigAffectsProxy(req, proxy) {
		return true
	}

	// 再看这个proxy是否有servive instance(也就是inbound)，并且配置有更新
	// 而且这个配置属于inbound服务更新。
	// If the proxy's service updated, need push for it.
	if len(proxy.ServiceInstances) > 0 && req.ConfigsUpdated != nil {
		svc := proxy.ServiceInstances[0].Service
		if _, ok := req.ConfigsUpdated[model.ConfigKey{
			Kind:      gvk.ServiceEntry,
			Name:      string(svc.Hostname),
			Namespace: svc.Attributes.Namespace,
		}]; ok {
			return true
		}
	}

	return false
}


// ConfigAffectsProxy 主要检测两个部分，一个部分是变更的配置kind是否影响指定的proxy类型。
// 另外一个就是根据SidecarScope看下这个配置是否在这个Proxy的影响范围。

func ConfigAffectsProxy(req *model.PushRequest, proxy *model.Proxy) bool {
	// Empty changes means "all" to get a backward compatibility.
	if len(req.ConfigsUpdated) == 0 {
		return true
	}

	for config := range req.ConfigsUpdated {
		affected := true

		// Some configKinds only affect specific proxy types
		if kindAffectedTypes, f := configKindAffectedProxyTypes[config.Kind]; f {
			affected = false
			for _, t := range kindAffectedTypes {
				if t == proxy.Type {
					affected = true
					break
				}
			}
		}

		if affected && checkProxyDependencies(proxy, config) {
			return true
		}
	}

	return false
}

func checkProxyDependencies(proxy *model.Proxy, config model.ConfigKey) bool {
	// Detailed config dependencies check.
	switch proxy.Type {
	case model.SidecarProxy:
		if proxy.SidecarScope.DependsOnConfig(config) {
			return true
		} else if proxy.PrevSidecarScope != nil && proxy.PrevSidecarScope.DependsOnConfig(config) {
			return true
		}
	default:
		// TODO We'll add the check for other proxy types later.
		return true
	}
	return false
}

// DependsOnConfig determines if the proxy depends on the given config.
// Returns whether depends on this config or this kind of config is not scoped(unknown to be depended) here.
func (sc *SidecarScope) DependsOnConfig(config ConfigKey) bool {
	if sc == nil {
		return true
	}
	// 集群纬度的配置，触发full push，比如envoy filter、Sidecar、健全等
	// This kind of config will trigger a change if made in the root namespace or the same namespace
	if _, f := clusterScopedConfigTypes[config.Kind]; f {
		return config.Namespace == sc.RootNamespace || config.Namespace == sc.Namespace
	}

	// 非service entry、
	// This kind of config is unknown to sidecarScope.
	if _, f := sidecarScopeKnownConfigTypes[config.Kind]; !f {
		return true
	}

	_, exists := sc.configDependencies[config.HashCode()]
	return exists
}
```

## Debug tools

```bash
curl http://127.0.0.1:9876/scopej/
curl 'http://127.0.0.1:9876/scopej/ads' \
  -X 'PUT' \
  -H 'Connection: keep-alive' \
  -H 'sec-ch-ua: "Chromium";v="88", "Google Chrome";v="88", ";Not A Brand";v="99"' \
  -H 'sec-ch-ua-mobile: ?0' \
  -H 'User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 11_1_0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/88.0.4324.96 Safari/537.36' \
  -H 'Content-Type: text/plain;charset=UTF-8' \
  -H 'Accept: */*' \
  -H 'Origin: http://127.0.0.1:9876' \
  -H 'Sec-Fetch-Site: same-origin' \
  -H 'Sec-Fetch-Mode: cors' \
  -H 'Sec-Fetch-Dest: empty' \
  -H 'Referer: http://127.0.0.1:9876/scopez/' \
  -H 'Accept-Language: zh-CN,zh;q=0.9,zh-TW;q=0.8,en;q=0.7' \
  --data-raw '{"name":"ads","description":"ads debugging","output_level":"info","stack_trace_level":"none","log_callers":true}' \
  --compressed
```
