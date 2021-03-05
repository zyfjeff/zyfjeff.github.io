
# WIP
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

	sortConfigByCreationTime(sidecarConfigs)samples/helloworld/helloworld.yaml

	// 统计带有Workselecor和不带Workselector的Sidecar config
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

	// 按照Namespace纬度组织SidecarScope
	ps.sidecarsByNamespace = make(map[string][]*SidecarScope, sidecarNum)
	for _, sidecarConfig := range sidecarConfigs {
		sidecarConfig := sidecarConfig
		ps.sidecarsByNamespace[sidecarConfig.Namespace] = append(ps.sidecarsByNamespace[sidecarConfig.Namespace],
			ConvertToSidecarScope(ps, &sidecarConfig, sidecarConfig.Namespace))
	}

	// 找到Root Namespace下的Sidecar config
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
	// 统计所有的Namespace
	namespaces := sets.NewSet()
	for _, nsMap := range ps.ServiceIndex.HostnameAndNamespace {
		for ns := range nsMap {
			namespaces.Insert(ns)
		}
	}
	// 在没有WorkSelector的Namespace下添加Root SidecarScope
	for ns := range namespaces {
		if _, exist := sidecarsWithoutSelectorByNamespace[ns]; !exist {
			ps.sidecarsByNamespace[ns] = append(ps.sidecarsByNamespace[ns], ConvertToSidecarScope(ps, rootNSConfig, ns))
		}
	}

	return nil
}
```

> 总结来说，带有WorkloadSelector优先

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

## Virtual Service

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
