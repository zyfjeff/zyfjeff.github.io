

# PushContext分析

## 什么是PushContext?

PushContext保存了一次Push中所涉及到的状态信息，下面是一个PushContext的组成，可以看到包含了所有Proxy的Status、

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

	// AuthnPolicies contains Authn policies by namespace.
	AuthnPolicies *AuthenticationPolicies `json:"-"`

	// AuthzPolicies stores the existing authorization policies in the cluster. Could be nil if there
	// are no authorization policies in the cluster.
	AuthzPolicies *AuthorizationPolicies `json:"-"`

	// The following data is either a global index or used in the inbound path.
	// Namespace specific views do not apply here.

	// Mesh configuration for the mesh.
	Mesh *meshconfig.MeshConfig `json:"-"`

	// Discovery interface for listing services and instances.
	ServiceDiscovery `json:"-"`

	// Config interface for listing routing rules
	IstioConfigStore `json:"-"`

	// PushVersion describes the push version this push context was computed for
	PushVersion string

	// LedgerVersion is the version of the configuration ledger
	LedgerVersion string

	// cache gateways addresses for each network
	// this is mainly used for kubernetes multi-cluster scenario
	networksMu      sync.RWMutex
	networkGateways map[string][]*Gateway

	initDone        atomic.Bool
	initializeMutex sync.Mutex
}
```


## PushContext何时创建和初始化?

* 初始创建Pilot Server的时候会去创建一个空的PushContext

```go
func NewServer(args *PilotArgs) (*Server, error) {
	e := &model.Environment{
		PushContext:  model.NewPushContext(),
		DomainSuffix: args.RegistryOptions.KubeOptions.DomainSuffix,
	}
    .....
}
```

这里创建的PushContext是用来作为全局的PushContext，被称为GlobalPushContext，倒是这个PushContext还没有进行初始化，还是一个空的。直到有请求到来的时候，才会触发其初始化。

```go
func (s *DiscoveryServer) Stream(stream DiscoveryStream) error {
    .....
	// InitContext returns immediately if the context was already initialized.
    // 当有请求的时候，开始进行初始化，初始化完成后会设置标志位，后续的请求将不会再进行初始化了。
	if err = s.globalPushContext().InitContext(s.Env, nil, nil); err != nil {
		// Error accessing the data - log and close, maybe a different pilot replica
		// has more luck
		adsLog.Warnf("Error reading config %v", err)
		return status.Error(codes.Unavailable, "error reading config")
	}
	con := newConnection(peerAddr, stream)
	con.Identities = ids
```



* 发生Push的时候，会创建一个新的PushContext，然后对GlobalPushContext进行更新。

```go
// Push is called to push changes on config updates using ADS. This is set in DiscoveryService.Push,
// to avoid direct dependencies.
func (s *DiscoveryServer) Push(req *model.PushRequest) {
	if !req.Full {
        // 不是全量Push就直接用global push context即可，因为相关资源不需要重新生成，直接对发生变化的资源做更新即可。
		req.Push = s.globalPushContext()
		s.AdsPushAll(versionInfo(), req)
		return
	}
    // 如果是全量Push，那么需要重新更新PushContext，这里先保存之前的PushContext方便排查请问题和debug
	// Reset the status during the push.
	oldPushContext := s.globalPushContext()
	if oldPushContext != nil {
		oldPushContext.OnConfigChange()
	}
	// PushContext is reset after a config change. Previous status is
	// saved.
	t0 := time.Now()

	versionLocal := time.Now().Format(time.RFC3339) + "/" + strconv.FormatUint(versionNum.Inc(), 10)
    // 开始创建新的PushContext，并使用global PushContext进行更新。
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

 通过下面的`initPushContext`方法，我们可以看到首先会去创建PushContext，然后调用`initContext`进行初始化。最后将更新后的PushContext重新赋值给Global Push Context

```go
// initPushContext creates a global push context and stores it on the environment. Note: while this
// method is technically thread safe (there are no data races), it should not be called in parallel;
// if it is, then we may start two push context creations (say A, and B), but then write them in
// reverse order, leaving us with a final version of A, which may be incomplete.
func (s *DiscoveryServer) initPushContext(req *model.PushRequest, oldPushContext *model.PushContext, version string) (*model.PushContext, error) {
    // 创建一个新的PushContext，然后用老的PushContext来初始化它
	push := model.NewPushContext()
	push.PushVersion = version
	if err := push.InitContext(s.Env, oldPushContext, req); err != nil {
		adsLog.Errorf("XDS: Failed to update services: %v", err)
		// We can't push if we can't read the data - stick with previous version.
		pushContextErrors.Increment()
		return nil, err
	}

	if err := s.UpdateServiceShards(push); err != nil {
		return nil, err
	}

	s.updateMutex.Lock()
    // 最后更新global push context
	s.Env.PushContext = push
	s.updateMutex.Unlock()

	return push, nil
}
```

在`InitContext`方法则会判断是否是做首次初始化，还是说使用global PushContext进行增量的初始化。

```go
// InitContext will initialize the data structures used for code generation.
// This should be called before starting the push, from the thread creating
// the push context.
func (ps *PushContext) InitContext(env *Environment, oldPushContext *PushContext, pushReq *PushRequest) error {
	// Acquire a lock to ensure we don't concurrently initialize the same PushContext.
	// If this does happen, one thread will block then exit early from initDone=true
	ps.initializeMutex.Lock()
	defer ps.initializeMutex.Unlock()
	if ps.initDone.Load() {
		return nil
	}

	ps.Mesh = env.Mesh()
	ps.ServiceDiscovery = env.ServiceDiscovery
	ps.IstioConfigStore = env.IstioConfigStore
	ps.LedgerVersion = env.Version()

	// Must be initialized first
	// as initServiceRegistry/VirtualServices/Destrules
	// use the default export map
	ps.initDefaultExportMaps()

	// create new or incremental update
    // 空的Push请求、不存在old push context、或者说old push context还没初始化，又或者push请求不包含任何配置请求
    // 这些情况都会导致全量的PushContext更新，否则就使用old push context做增量更新
	if pushReq == nil || oldPushContext == nil || !oldPushContext.initDone.Load() || len(pushReq.ConfigsUpdated) == 0 {
		if err := ps.createNewContext(env); err != nil {
			return err
		}
	} else {
        // 增量更新
		if err := ps.updateContext(env, oldPushContext, pushReq); err != nil {
			return err
		}
	}

	// TODO: only do this when meshnetworks or gateway service changed
	ps.initMeshNetworks(env.Networks())

	ps.initClusterLocalHosts(env)

	ps.initDone.Store(true)
	return nil
}
```


## PushContext如何初始化?

```go
func (ps *PushContext) createNewContext(env *Environment) error {
	if err := ps.initServiceRegistry(env); err != nil {
		return err
	}

	if err := ps.initVirtualServices(env); err != nil {
		return err
	}

	if err := ps.initDestinationRules(env); err != nil {
		return err
	}

	if err := ps.initAuthnPolicies(env); err != nil {
		return err
	}

	if err := ps.initAuthorizationPolicies(env); err != nil {
		authzLog.Errorf("failed to initialize authorization policies: %v", err)
		return err
	}

	if err := ps.initEnvoyFilters(env); err != nil {
		return err
	}

	if err := ps.initGateways(env); err != nil {
		return err
	}

	// Must be initialized in the end
	if err := ps.initSidecarScopes(env); err != nil {
		return err
	}
	return nil
}
```
