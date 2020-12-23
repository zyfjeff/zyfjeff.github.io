
# Programming Kubernetes

## Kubernetes提供的扩展机制:

* `cloud-controller-manager` 对接各个云厂商提供的能力，比如Load Balancer、VM等
* `Binary kubectl plug-ins` 通过二进制扩展kubelet子命令
* `Binary kubelet plug-ins` 通过二进制扩展网络、存储、容器运行时等
* `Access extensions in the API server` 比如dynamic admission control with webhooks
* `Custom resources` 和 `custom controllers`
* `Custom API servers`
* Scheduler externsions
* Authentication with webhooks


## controller

“a controller implements a control loop, watching the shared state of the cluster through the API server and making changes in an attempt to move the current state toward the desired state.”

![controll-loop](images/controll-loop.jpg)

1. 读取资源的状态(更可取的方式是通过事件驱动的方式来读取)
2. 改变集群中对象的状态，比如启动一个POD、创建一个网络端点、查询一个cloud API等
3. 通过API server来更新Setp1中的资源状态(Optimistic Concurrency)
4. 循环重复，返回到Setp1

核心数据结构:

1. informers 提供一种扩展、可持续的方式来查看资源的状态，并实现了resync机制(强制执行定期对帐，通常用于确保群集状态和缓存在内存中的假定状态不会漂移)
2. Work queues 用于将状态变化的事件进行排队处理，便于去实现重试(发生错误的时候，重新投入到队列中)


Kubernetes does not determine a calculated, coordinated sequence of commands to execute based on the current state and the desired state.

Kubernetes并不会根据当前的状态和预期的状态来计算达到预期状态所需要的命令序列，从而来实现所谓的声明式系统，相反Kubernetes仅仅会根据当前的状态计算出下一个命令，如果没有可用的命令，则Kubernetes就达到稳态了


Ref:
* [The Mechanics of Kubernetes](https://medium.com/@dominik.tornow/the-mechanics-of-kubernetes-ac8112eaa302)
* [A deep dive into Kubernetes controllers](https://engineering.bitnami.com/articles/a-deep-dive-into-kubernetes-controllers.html)


## Events

Kubernetes中大量使用事件和一些松耦合的组件。其他的一些分布式系统主要是RPC来触发行为，但是Kubernetes没有这么做。
Kubernetes控制器通过监控Kubernetes对象在API server中的改变(添加、删除、更新)等。当这些事件发生，Kubernetes控制器执行对应的业务逻辑。

下面是一个POD创建的过程:

1. deployment控制器(属于controller-manager组件)通过deployment informer发现了一个deployment创建的事件，于是开始创建一个replica set
2. replica set控制器(属于controller-manager组件)通过replica set informer发现一个新的replica set创建，于是开始创建一个POD对象
3. scheduler(属于kube-scheduler组件)，他也是一个控制器，通过pod informer发现了一个POD对象的创建，并且spec.nodeName是空，于是就将其放到了scheduling的队列中
4. kubelet(也是一个控制器)同样也发现了一个新的POD对象的创建，但是spec.nodeName是空，和自己的node name并不一致，因此就忽略了这个事件，继续sleep
5. scheduler从队列中获取一个POD，并更新spec.nodeName字段，填入要调度到的nodeName，写入到API server
6. kubelect组件因为POD状态发生了改变被唤醒，通过比较spec.nodeName和自己的nodeName，如果匹配到了，就根据POD对象创建对应的容器。并根据容器引擎的执行情况来更新POD状态
7. replica set控制器发现POD变化了额，但是什么也没做
8. 最终POD terminates，kubelet会收到通知，并获取POD对象，更新其状态为terminated。
9. replica set控制器发现POD的状态为terminated，于是删除POD对象，重新创建一个新的
10. 到此结束


所有的controller本身是无状态的，便于水平扩展，尽管他们会去执行一些有状态的操作，Controller重启后会重放所有的事件(或者定期resync的时候)，类似event-sourcing机制
Controllers essentially are stateless even though they perform stateful operations

水平触发 + resync + optimistic concurrency 是Kubernetes 事件驱动架构的核心

* Controller会定期进行resync以此来避免因为bug或者停机导致事件丢失
* 通过乐观并发来解决并发更新的问题
* 事件队列的优点: 解耦、背压、通过事件重放来做审计和debug

![optimistic-concurrent](images/optimistic_concurrent.jpg)


Ref:
* [深入浅出Event Sourcing和CQRS](https://zhuanlan.zhihu.com/p/38968012)
* [Events, the DNA of Kubernetes](https://www.mgasch.com/post/k8sevents/)
* [QoS, "Node allocatable" and the Kubernetes Scheduler](https://www.mgasch.com/post/sched-reconcile/)


## Level triger vs Edge triger


* “Edge-driven triggers
At the point in time the state change occurs, a handler is triggered—for example, from no pod to pod running.

* Level-driven triggers
The state is checked at regular intervals and if certain conditions are met (for example, pod running), then a handler is triggered.”

后者不具备可扩展性，本质上是polling、polling的间隔会影响controller的实时性。

三种触发策略:

1. Edge driven trigger 没有处理事件丢失的问题
2. Edge driven trigger + Level-driven triggers，总是去获取最新的状态(当有事件来的时候)，因此即使丢失一个事件，仍然可以通过获取最新的状态来进行业务逻辑
3. Edge driven trigger + Level-driven triggers + resync 如果最后一个事件丢失了，后面没有事件来了，所以也不会去触发(Level-driven triggers)，这个时候需要借助resync来得到最新的状态。

![event driven trigger](images/event-trigger.png)

kubernetes实现了上面的第三种策略

Ref:
* [level-triggering-and-reconciliation-in-kubernetes](https://hackernoon.com/level-triggering-and-reconciliation-in-kubernetes-1f17fe30333d)


## Optimistic Concurrency

“Our solution is a new parallel scheduler architecture built around shared state, using lock-free optimistic concurrency control, to achieve both implementation extensibility and performance scalability. 
This architecture is being used in Omega, Google’s next-generation cluster management system.”

我们的解决方案是基于共享状态构建的新的并行调度器体系结构，使用无锁乐观并发控制，以实现可扩展性和性能可伸缩性。这种架构正在谷歌的下一代集群管理系统Omega中使用


```go
var err error
for retries := 0; retries < 10; retries++ {
    foo, err = client.Get("foo", metav1.GetOptions{})
    if err != nil {
        break
    }

    <update-the-world-and-foo>

	_, err = client.Update(foo)
	// 通过资源版本(ObjectMeta字段中的resource version)来判断当前是否有人在写入，是否会产生并发写的冲突
    if err != nil && errors.IsConflict(err) {
        continue
    } else if err != nil {
        break
    }
}
```

conflict errors are totally normal in controllers. Always expect them and handle them gracefully.
冲突错误在控制器中是完全正常的。我们应该总是预期它们会出现，并优雅地处理它们

![optimistic-concurrency](images/optimistic-concurrency.png)

## Operator

A Site Reliability Engineer (SRE) is a person [who] operates an application by writing software. They are an engineer, a developer, who knows how to develop software specifically for a particular application domain. The resulting piece of software has an application’s operational domain knowledge programmed into it.

We call this new class of software Operators. An Operator is an application-specific controller that extends the Kubernetes API to create, configure, 
and manage instances of complex stateful applications on behalf of a Kubernetes user. It builds upon the basic Kubernetes resource and controller concepts 
but includes domain or application-specific knowledge to automate common tasks.

1. There’s some domain-specific operational knowledge you’d like to automate.

2. The best practices for this operational knowledge are known and can be made explicit—for example, 
   in the case of a Cassandra operator, when and how to re-balance nodes, or in the case of an operator for a service mesh, how to create a route.

3. The artifacts shipped in the context of the operator are
	1. A set of custom resource definitions (CRDs) capturing the domain-specific schema and 
	   custom resources following the CRDs that, on the instance level, represent the domain of interest.

	2. A custom controller, supervising the custom resources, potentially along with core resources. For example, the custom controller might spin up a pod.

Operator就是一个具有领域知识的用于运维的controller，一般来说，这个controller通过一组具有领域知识的schema组成的crd来实现自动化运维。

![optimistic-concurrency](images/operator-concept.png)

Ref:
[introducing-operators](https://coreos.com/blog/introducing-operators.html)
## API

“In Kubernetes programs, a kind directly corresponds with a Golang type.”

Kind: 实体的类型，每一个对象都有一个Kind字段，kind主要有三类。
	1. 表示一个持久化的实体对象，比如Pod、Endpoints等
	2. 一个或多个kind实体，比如PodList、NodeLists等
	3. 特殊目的，比如binding、scale
API Group: 一堆kind的逻辑上所属集合，利润
Version: API group或者是对象的版本，一个group或对象可以存在多个版本。
Resource: 通常小写、复数形式(pods) 用来识别一系列的HTTP endpoints路径，用来暴露对象的CRUD语义，例如: .../pods/nginx 查看名为nginx的pod

所有的主resource都是具有CRUD语义的，但是也存在一些response可以支持更多的action，比如.../pod/nginx/port-forward，这个时候我们称之为Subresources。这需要通过自定义协议来替代RESET。
例如exec是通过WebSockets来实现的。

在Kubernetes中，kind是直接映射到一个Golang类型的。

> Resources和Kind是相互的，Resources指定HTTP endpoints，而Kind是这个endpoints返回对象的类型，也是etcd中持久化的对象。
> 每一个对象都可以按照version v1来表示，也可以按照v1beta1来表示，可以返回不同的版本

Resources是API group和version的一部分，三者被称之为GVR(GroupVersionResource)，一个GVR唯一标示一个HTTP path。例如: `/apis/batch/v1/namespaces/default/jobs`
通过GVR可以获取到类型为kind的对象，同理这个对象也是属于这个version和Group的。因此称之为GVK(GroupVersionKind)

> 核心组/api/v1，命名组/apis/$name/$version，，为什么核心组不是/apis/core/v1呢? 这是因为历史原因导致的，API Group是核心组之后引入的。

![gvr](images/gvr.jpg)


通过下面两种方式来访问资源

```bash
kubectl proxy --port=8080
curl http://127.0.0.1:8080/apis/batch/v1

或

kubectl get --raw /apis/batch/v1
```

Apiserver是如何处理请求的:
1. 首先HTTP request会被`DefaultBuildHandlerChain`注册的filters chain来处理
2. 接着根据HTTP path走到分发器，通过分发器来路由到最终的handler
3. 每一个gvr都会注册一个handler

基本上一个请求会经过以下几个阶段:
1. 鉴权
2. admission
	1. mutating phase
	2. schemla validation
3. validation

multaing webhooks、validation webhooks就可以hook上面两个阶段。

![apiserver process](images/apiserver-process.jpg)


## Object

Kubernetes中的对象，其基础接口如下:

```golang
// Object interface must be supported by all API types registered with Scheme. Since objects in a scheme are
// expected to be serialized to the wire, the interface an Object must provide to the Scheme allows
// serializers to set the kind, version, and group the object is represented as. An Object may choose
// to return a no-op ObjectKindAccessor in cases where it is not expected to be serialized.
type Object interface {
	GetObjectKind() schema.ObjectKind
	DeepCopyObject() Object
}


// All objects that are serialized from a Scheme encode their type information. This interface is used
// by serialization to set type information from the Scheme onto the serialized version of an object.
// For objects that cannot be serialized or have unique requirements, this interface may be a no-op.
type ObjectKind interface {
	// SetGroupVersionKind sets or clears the intended serialized kind of an object. Passing kind nil
	// should clear the current setting.
	SetGroupVersionKind(kind GroupVersionKind)
	// GroupVersionKind returns the stored group, version, and kind of an object, or nil if the object does
	// not expose or provide these fields.
	GroupVersionKind() GroupVersionKind
}
```

> 每一个对象都要满足Object接口的约束，可以看出通过这些接口可以设置和获取对象的GVK，以及进行深度拷贝。

每一个Kubernetes对象都会嵌入一个`metav1.TypeMeta struct`。还有一个`metav1.ObjectMeta`

```golang
// TypeMeta describes an individual object in an API response or request
// with strings representing the type of the object and its API schema version.
// Structures that are versioned or persisted should inline TypeMeta.
//
// +k8s:deepcopy-gen=false
type TypeMeta struct {
	// Kind is a string value representing the REST resource this object represents.
	// Servers may infer this from the endpoint the client submits requests to.
	// Cannot be updated.
	// In CamelCase.
	// More info: https://git.k8s.io/community/contributors/devel/api-conventions.md#types-kinds
	// +optional
	Kind string `json:"kind,omitempty" protobuf:"bytes,1,opt,name=kind"`

	// APIVersion defines the versioned schema of this representation of an object.
	// Servers should convert recognized schemas to the latest internal value, and
	// may reject unrecognized values.
	// More info: https://git.k8s.io/community/contributors/devel/api-conventions.md#resources
	// +optional
	APIVersion string `json:"apiVersion,omitempty" protobuf:"bytes,2,opt,name=apiVersion"`
}

type ObjectMeta struct {
    Name string `json:"name,omitempty"`
    Namespace string `json:"namespace,omitempty"`
    UID types.UID `json:"uid,omitempty"`
    ResourceVersion string `json:"resourceVersion,omitempty"`
    CreationTimestamp Time `json:"creationTimestamp,omitempty"`
    DeletionTimestamp *Time `json:"deletionTimestamp,omitempty"`
    Labels map[string]string `json:"labels,omitempty"`
    Annotations map[string]string `json:"annotations,omitempty"`
    ...
}
```

> 每一个对象都会包含一个`metav1.TypeMeta struct`字段和一个`metav1.ObjectMeta`字段
> 前者用来描述一个对象，比如是什么kind，什么APIVersion，后者则是一些元信息，比如label、注解、其中ResourceVersion就是用来实现乐观并发(optimistic-concurrency)等

一个POD对象如下:

```golang
// Pod is a collection of containers that can run on a host. This resource is created
// by clients and scheduled onto hosts.
type Pod struct {
	metav1.TypeMeta `json:",inline"`
	// Standard object's metadata.
	// More info: https://git.k8s.io/community/contributors/devel/api-conventions.md#metadata
	// +optional
	metav1.ObjectMeta `json:"metadata,omitempty" protobuf:"bytes,1,opt,name=metadata"`

	// Specification of the desired behavior of the pod.
	// More info: https://git.k8s.io/community/contributors/devel/api-conventions.md#spec-and-status
	// +optional
	Spec PodSpec `json:"spec,omitempty" protobuf:"bytes,2,opt,name=spec"`

	// Most recently observed status of the pod.
	// This data may not be up to date.
	// Populated by the system.
	// Read-only.
	// More info: https://git.k8s.io/community/contributors/devel/api-conventions.md#spec-and-status
	// +optional
	Status PodStatus `json:"status,omitempty" protobuf:"bytes,3,opt,name=status"`
}
```

每一个对象都有自己独立的Spec和Status，通常Spec表示用户的期望，Status则是期望的的结果，是Controller和Operator来负责填充。也存在一些异常，比如endpoints和RBAC


> 请求分为长请求和短请求，对于长请求一般来说是诸如watch、一些Subresources(exec、sport-forward)等，对于短请求则会有60s的超时时间，当API server下线的时候会等待60s直到服务完这些短请求
> 对于长请求则直接断掉。从而实现所谓的优雅关闭。


## Informers and Caching

Informers通过watch接口实现Cachae和增量更新。并能够很好的处理网络抖动，断网等场景。尽可能的每一种资源类型只创建一个Informers，否则会导致资源的浪费，为此可以通过`InformerFactory`来创建Informer。
他内部对于同一个资源类型只会创建一个informer实例。

It is very important to remember that any object passed from the listers to the event handlers is owned by the informers. If you mutate it in any way, you risk introducing hard-to-debug cache coherency issues into your application. Always do a deep copy (see “Kubernetes Objects in Go”) before changing an object.

我们在informers的event回调中切记不要修改对象，否则会导致很难排查的缓存一致性问题，如果要修改的化，请先深拷贝，然后修改。

```go
	informerFactory := informers.NewSharedInformerFactory(clientset, time.Second*30)
	podInformer := informerFactory.Core().V1().Pods()
	podInformer.Informer().AddEventHandler(cache.ResourceEventHandlerFuncs{
		AddFunc:    func(new interface{}) { fmt.Println("Create a pod") },
		UpdateFunc: func(old, new interface{}) { fmt.Println("Update a pod") },
		DeleteFunc: func(obj interface{}) { fmt.Println("Delete a pod") },
	})
	informerFactory.Start(wait.NeverStop)
	informerFactory.WaitForCacheSync(wait.NeverStop)
	pod, _ := podInformer.Lister().Pods("default").Get("details-v1-5974b67c8-n7vdw")
```

默认informer会监听所有namespace下的指定资源，也可以通过`NewSharedInformerFactoryWithOptions`来进行过滤

```go
informerFactory := informers.NewSharedInformerFactoryWithOptions(clientset, time.Second*30, informers.WithNamespace("default"))
```

* object owner

“In general: before mutating an object, always ask yourself who owns this object or the data structures in it. As a rule of thumb:”

1. “Informers and listers own objects they return. Hence, consumers have to deep-copy before mutation.”
2. “Clients return fresh objects, which the caller owns.”
3. “Conversions return shared objects. If the caller does own the input object, it does not own the output.”

![informers](images/informers.jpg)

## Glang type、GroupVersionKind、GroupVerisonResource、HTTP path、Resources

* Kinds

1. 在kubernetes中，每一个GVK对应到一个具体的Go类型，相反，一个Go类型可以对应到多个GVK
2. Kind并不会一对一和HTTP paths mapping，可能多个kind对应一个HTTP path，也有可能一个也不对应。

> 例如: admission.k8s.io/v1beta1.AdmissionReview不对应任何HTTP path
> 例如: meta.k8s.io/v1.Status 对应很多HTTP path

按照约定kind的命名按照驼峰命名，并且使用单数，而且对于CustomResourceDefinition类型的资源，其kind必须是符合DNS path label(REF 1035)

* Resources

代表一类资源，这个资源是属于一个group，并且是有版本的，所以就有了GVR(GroupVersionResource)，每一个GVR都会对应到一个HTTP path。
通过GVR来应识别出一个Kubernetes的 REST API endpoint，例如GVR apps/v1.deloyment会映射到/apis/apps/v1/namespace/NAMESPACE/deployments
客户端就是通过这种映射关系来构建HTTP path的。

> 一个GVR是否是namespaced，或者是集群级别的，这是需要知道的，否则无法构建HTTP Path，按照约定，小写，并且是复数类型的kind，并且遵从DNS path label format
> 那么这个GVR对应的就是一个集群级别的。直接用kind来映射到HTTP path。
> 例如: rbac.authorization.k8s.io/v1.clusterroles，映射到HTTP path就是apis/rbac.authorization.k8s.io/v1/clusterroles.


## Scheme
虽然每一个Object都会包含`TypeMeta`，里面包含了Kind和APIVersion但是实际上，我们去访问的时候，并不会拿到对应的信息，这些信息都是空的，相反我们需要通过scheme来获取对象的Kind

```go
func (s *Scheme) ObjectKinds(obj Object) ([]schema.GroupVersionKind, bool, error)
```

通过Scheme来连接GVK和Golang类型，Scheme对两者做了映射，Scheme会预先对大量内置类型做映射。

```go
scheme.AddKnownTypes(schema.GroupVersionKind{"", "v1", "Pod"}, &Pod{})
```

最终一个Golang类型，通过shceme映射到GVK，然后GVK通过RESTMapper映射到GVR，最终通过GVR拼接出HTTP path。

```golang
// RESTMapping contains the information needed to deal with objects of a specific
// resource and kind in a RESTful manner.
type RESTMapping struct {
	// Resource is the GroupVersionResource (location) for this endpoint
	Resource schema.GroupVersionResource

	// GroupVersionKind is the GroupVersionKind (data format) to submit to this endpoint
	GroupVersionKind schema.GroupVersionKind

	// Scope contains the information needed to deal with REST Resources that are in a resource hierarchy
	Scope RESTScope
}
```

![k8s-scheme](images/k8s-scheme.jpg)

## Subresources

Subresources就是一个特殊的HTTP endpoints，一般是在正常resource后面添加的一个后缀来表示，比如，对于pod资源来说，正常的HTTP Path是
 `/api/v1/namespace/namespace/pods/name`，他可以有多个Subresources，比如`/logs`、`/portforward`、`/exec`、`status`。

Subresources所使用的协议是和主资源不一样的，目前为止自定义资源支持/scale和/status两种子资源。

/status子资源的目的是为了和将spec和status进行分离，

## CustomResourceDefinition

* 如何找到自定义资源
1. 通过/apis询问Api server所有的 API group
2. 通过/apis/group/version 查看所有的group存在的资源，找到对应资源所在的Group、VersionheResources

```yaml
apiVersion: apiextensions.k8s.io/v1beta1
kind: CustomResourceDefinition
metadata:
  name: ats.cnat.programming-kubernetes.info
spec:
  additionalPrinterColumns: (optional)
  - name: kubectl column name
    type: OpenAPI type for the column
    format: OpenAPI format for the column (optional)
    description: human-readable description of the column (optional)
    priority: integer, always zero supported by kubectl
    JSONPath: JSON path inside the CR for the displayed value
```

每一个自定义资源都可以有subresourcs，默认支持scale、status两类子资源

```yaml
KIND:     CustomResourceDefinition
VERSION:  apiextensions.k8s.io/v1beta1

RESOURCE: subresources <Object>

DESCRIPTION:
     Subresources describes the subresources for CustomResource Optional, the
     global subresources for all versions. Top-level and per-version
     subresources are mutually exclusive.

     CustomResourceSubresources defines the status and scale subresources for
     CustomResources.

FIELDS:
   scale        <Object>
     Scale denotes the scale subresource for CustomResources

   status       <map[string]>
     Status denotes the status subresource for CustomResources
```

自定义资源可以通过几种方式来访问:

1. 使用client-go dynamic client(自定义类型通过Unstructured来表示)

```golang
schema.GroupVersionResource{
  Group: "apps",
  Version: "v1",
  Resource: "deployments",
}
client, err := NewForConfig(cfg)
client.Resource(gvr).Namespace(namespace).Get("foo", metav1.GetOptions{})
```
输入和输出都是`unstructured.Unstructured`，本质上是一个`json.Unmarshal`后的结构，用来保存对象。

  * Object通过map[string]interface{}来表示
  * 数组通过[]intreface{}来标志
  * string、bool、float64、int64是基本类型

例如NestedString来获取一个对象中的某个字段，其实就是遍历对象map，找到对应key的value，然后做类型断言，如果找到了就返回。

```golang

type Unstructured struct {
	// Object is a JSON compatible map with string, float, int, bool, []interface{}, or
	// map[string]interface{}
	// children.
	Object map[string]interface{}
}

// NestedFieldNoCopy returns a reference to a nested field.
// Returns false if value is not found and an error if unable
// to traverse obj.
func NestedFieldNoCopy(obj map[string]interface{}, fields ...string) (interface{}, bool, error) {
	var val interface{} = obj

	for i, field := range fields {
		if m, ok := val.(map[string]interface{}); ok {
			val, ok = m[field]
			if !ok {
				return nil, false, nil
			}
		} else {
			return nil, false, fmt.Errorf("%v accessor error: %v is of the type %T, expected map[string]interface{}", jsonPath(fields[:i+1]), val, val)
		}
	}
	return val, true, nil
}

// NestedString returns the string value of a nested field.
// Returns false if value is not found and an error if not a string.
func NestedString(obj map[string]interface{}, fields ...string) (string, bool, error) {
	val, found, err := NestedFieldNoCopy(obj, fields...)
	if !found || err != nil {
		return "", found, err
	}
	s, ok := val.(string)
	if !ok {
		return "", false, fmt.Errorf("%v accessor error: %v is of the type %T, expected string", jsonPath(fields), val, val)
	}
	return s, true, nil
}
```

2. 使用typed client:
  * kubernetes-sigs/controller-runtime 和 kubebuilder
  * 通过client-gen来生成

typed client不使用类似于`map [string] interface {}`的通用数据结构，而是使用实际的Golang类型，这些类型对于每个GVK都是不同的和特定的

通过`scheme.AddKnownTypes`进行类型的注册，本质上是构建了一个gvk到对应类型的映射，以及type到gvk的映射表。

```golang
var (
	// TODO: move SchemeBuilder with zz_generated.deepcopy.go to k8s.io/api.
	// localSchemeBuilder and AddToScheme will stay in k8s.io/kubernetes.
	SchemeBuilder      = runtime.NewSchemeBuilder(addKnownTypes)
	localSchemeBuilder = &SchemeBuilder
	AddToScheme        = localSchemeBuilder.AddToScheme
)

// Adds the list of known types to the given scheme.
func addKnownTypes(scheme *runtime.Scheme) error {
	scheme.AddKnownTypes(SchemeGroupVersion,
		&Deployment{},
		&DeploymentList{},
		&StatefulSet{},
		&StatefulSetList{},
		&DaemonSet{},
		&DaemonSetList{},
		&ReplicaSet{},
		&ReplicaSetList{},
		&ControllerRevision{},
		&ControllerRevisionList{},
	)
	metav1.AddToGroupVersion(scheme, SchemeGroupVersion)
	return nil
}

// AddKnownTypes registers all types passed in 'types' as being members of version 'version'.
// All objects passed to types should be pointers to structs. The name that go reports for
// the struct becomes the "kind" field when encoding. Version may not be empty - use the
// APIVersionInternal constant if you have a type that does not have a formal version.
func (s *Scheme) AddKnownTypes(gv schema.GroupVersion, types ...Object) {
	s.addObservedVersion(gv)
	for _, obj := range types {
		t := reflect.TypeOf(obj)
		if t.Kind() != reflect.Ptr {
			panic("All types must be pointers to structs.")
		}
		t = t.Elem()
		s.AddKnownTypeWithName(gv.WithKind(t.Name()), obj)
	}
}

// AddKnownTypeWithName is like AddKnownTypes, but it lets you specify what this type should
// be encoded as. Useful for testing when you don't want to make multiple packages to define
// your structs. Version may not be empty - use the APIVersionInternal constant if you have a
// type that does not have a formal version.
func (s *Scheme) AddKnownTypeWithName(gvk schema.GroupVersionKind, obj Object) {
	s.addObservedVersion(gvk.GroupVersion())
	t := reflect.TypeOf(obj)
	if len(gvk.Version) == 0 {
		panic(fmt.Sprintf("version is required on all types: %s %v", gvk, t))
	}
	if t.Kind() != reflect.Ptr {
		panic("All types must be pointers to structs.")
	}
	t = t.Elem()
	if t.Kind() != reflect.Struct {
		panic("All types must be pointers to structs.")
	}

	if oldT, found := s.gvkToType[gvk]; found && oldT != t {
    panic(fmt.Sprintf("Double registration of different types for %v: old=%v.%v, new=%v.%v in scheme %q",
                       gvk, oldT.PkgPath(), oldT.Name(), t.PkgPath(), t.Name(), s.schemeName))
	}

	s.gvkToType[gvk] = t

	for _, existingGvk := range s.typeToGVK[t] {
		if existingGvk == gvk {
			return
		}
	}
	s.typeToGVK[t] = append(s.typeToGVK[t], gvk)
}

```



## Automating Code Generation
Golang是一个简单的语言，缺乏类似于元编程的机制来给不同的类型实现算法，因此go选择使用代码生成的方式来实现。

Kubernetest通过代码给每一类资源生成一些方法。主要有四种标准的代码生成。

1. `deepcopy-gen` 生成`func (t *T) DeepCopy() *T` 和 `func (t* T)DeepCopyInto(*T)`两个方法
2. `client-gen` 创建带类型的client sets
3. `informer-gen`
4. `lister-gen`

通过上面四种代码生成器可以构建一个强大的控制器，除此之外还有`conversion-gen`和`defaulter-gen`两个生成器给编写`aggregated API server`提供便利。

> `k8s.io/code-generator` 代码生成器仓库，通过`generate-groups.sh`来触发代码生成。

```shell
set -o pipefail

# 确保k8s.io/code-generator已经在vendor中了
SCRIPT_ROOT=$(dirname "${BASH_SOURCE[0]}")/..
CODEGEN_PKG=${CODEGEN_PKG:-$(cd "${SCRIPT_ROOT}"; ls -d -1 ./vendor/k8s.io/code-generator 2>/dev/null || echo ../code-generator)}

# generate the code with:
# --output-base    because this script should also be able to run inside the vendor dir of
#                  k8s.io/kubernetes. The output-base is needed for the generators to output into the vendor dir
#                  instead of the $GOPATH directly. For normal projects this can be dropped.

# 调用k8s.io/code-generator中的generate-groups.sh脚本并指定参数
# 1. 指定生成器的类型
# 2. 生成的代码所属于的package name(client、informer、lister)
# 3. API group的package name
# 4. 要生成的 API group和Version，可以有多个，group:version格式。
# --output-base 定于生成的代码的基目录
# --go-header-file 生成的文件是否放入copyright内容
# deepcoy-gen生成器是直接在API group package中生成的。默认生成的文件是zz_generated前缀。
bash "${CODEGEN_PKG}"/generate-groups.sh "deepcopy,client,informer,lister" \
  k8s.io/sample-controller/pkg/generated k8s.io/sample-controller/pkg/apis \
  samplecontroller:v1alpha1 \
  --output-base "$(dirname "${BASH_SOURCE[0]}")/../../.." \
  --go-header-file "${SCRIPT_ROOT}"/hack/boilerplate.go.txt

# To use your own boilerplate text append:
#   --go-header-file "${SCRIPT_ROOT}"/hack/custom-boilerplate.go.txt
```

这些生成器如何生成代码是可以通过命令行参数来控制，也可以细粒度通过在代码中打tag的方式来控制，主要有两类tag

1. global tags，通常在一个pakcage中的doc.go文件中。

```golang
+k8s:deepcopy-gen=package			// 给整个pakcgae中的类型都进行deepcopy类型的代码生成
+groupName=samplecontroller.k8s.io	// 指定API group name的全称，默认用的是parent的package name
```

2. local tags，通常在一个struct类型定义上。

```golang
// +genclient
// +k8s:deepcopy-gen:interfaces=k8s.io/apimachinery/pkg/runtime.Object	// 不仅仅生成DeepCopy和DeepCopyInto方法，还要生成k8s.io/apimachinery/pkg/runtime.Object接口。
```



## Solutions for Writing OPerators

###  Controller编程基础

1. ownerReference，一个属主具有多个附属对象，每一个附属对象具有ownerReference指向其属主对象。

> 根据设计，kubernetes 不允许跨命名空间指定属主。这意味着： 
> 1）命名空间范围的附属只能指定同一的命名空间中的或者集群范围的属主。 
> 2）集群范围的附属只能指定集群范围的属主，不能指定命名空间范围的属主。

当你删除对象时，可以指定该对象的附属是否也自动删除。 自动删除附属的行为也称为 级联删除（Cascading Deletion）。
Kubernetes 中有两种 级联删除 模式：后台（Background） 模式和 前台（Foreground） 模式。这个删除策略是可以通过
属主对象的`deleteOptions.propagationPolicy`来控制的。 可能的取值包括：`Orphan`、`Foreground` 或者 `Background`。
在 后台级联删除 模式下，Kubernetes 会立即删除属主对象，之后垃圾收集器 会在后台删除其附属对象。
在 前台级联删除 模式下，根对象首先进入 `deletion in progress` 状态。 在 `deletion in progress` 状态，会有如下的情况：

1. 对象仍然可以通过 REST API 可见。
2. 对象的 deletionTimestamp 字段被设置。
3. 对象的 metadata.finalizers 字段包含值 foregroundDeletion

一旦对象被设置为`deletion in progress` 状态，垃圾收集器会删除对象的所有附属。 垃圾收集器在删除了所有有阻塞能力的附属（对象的 ownerReference.blockOwnerDeletion=true） 之后，删除属主对象。
如果一个对象的 ownerReferences 字段被一个控制器（例如 Deployment 或 ReplicaSet）设置， blockOwnerDeletion 也会被自动设置，你不需要手动修改这个字段。


如果删除对象时，不自动删除它的附属，这些附属被称作 孤立对象（Orphaned） 。

> 默认是级联删除，通过--cascade可以控制这个行为
> kubectl delete  replicaset my-repset --cascade=false 这样my-repset其附属管理的pod对象就不会被删除了。


下面的代码在创建了Pod后，将这个pod设置Controller的附属对象

```golang
pod := newPodForCR(instance)
// Set At instance as the owner and controller
owner := metav1.NewControllerRef(instance, cnatv1alpha1.SchemeGroupVersion.WithKind("At"))
pod.ObjectMeta.OwnerReferences = append(pod.ObjectMeta.OwnerReferences, *owner)
```

2. Create Controller

```golang
import (
	"flag"
	"os"
	"path/filepath"
	"time"

	"k8s.io/apimachinery/pkg/util/wait"
	kubeinformers "k8s.io/client-go/informers"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"
	"k8s.io/klog"

	clientset "github.com/programming-kubernetes/cnat/cnat-client-go/pkg/generated/clientset/versioned"
	informers "github.com/programming-kubernetes/cnat/cnat-client-go/pkg/generated/informers/externalversions"
)

var (
	masterURL  string
	kubeconfig string
)

func main() {
	flag.StringVar(&kubeconfig, "kubeconfig", defaultKubeconfig(), "Path to a kubeconfig. Only required if out-of-cluster.")
	flag.StringVar(&masterURL, "master", "", "The address of the Kubernetes API server. Overrides any value in kubeconfig. Only required if out-of-cluster.")

	klog.InitFlags(nil)

	flag.Parse()
	// 先探测集群内的配置，如果存在就使用这个配置。否则就使用默认的kubeconfig.
	cfg, err := rest.InClusterConfig()
	if err != nil {
		cfg, err = clientcmd.BuildConfigFromFlags(masterURL, kubeconfig)
		if err != nil {
			klog.Fatalf("Error building kubeconfig: %s", err.Error())
		}
	}
	// 通过配置构建kube client
	kubeClient, err := kubernetes.NewForConfig(cfg)
	if err != nil {
		klog.Fatalf("Error building kubernetes clientset: %s", err.Error())
	}
	// 创建自定义资源的client
	cnatClient, err := clientset.NewForConfig(cfg)
	if err != nil {
		klog.Fatalf("Error building cnat clientset: %s", err.Error())
	}

	// 创建kubeInfomerFactory
	kubeInformerFactory := kubeinformers.NewSharedInformerFactory(kubeClient, time.Minute*10)
	// 创建自定义资源的InfomerFactory
	cnatInformerFactory := informers.NewSharedInformerFactory(cnatClient, time.Minute*10)

	// 创建Controller
	controller := NewController(kubeClient, cnatClient, cnatInformerFactory.Cnat().V1alpha1().Ats(), kubeInformerFactory.Core().V1().Pods())

	// 启动infomer
	// notice that there is no need to run Start methods in a separate goroutine. (i.e. go kubeInformerFactory.Start(stopCh))
	// Start method is non-blocking and runs all registered informers in a dedicated goroutine.
	kubeInformerFactory.Start(wait.NeverStop)
	cnatInformerFactory.Start(wait.NeverStop)

	if err = controller.Run(2, wait.NeverStop); err != nil {
		klog.Fatalf("Error running controller: %s", err.Error())
	}
}

func defaultKubeconfig() string {
	fname := os.Getenv("KUBECONFIG")
	if fname != "" {
		return fname
	}
	home, err := os.UserHomeDir()
	if err != nil {
		klog.Warningf("failed to get home directory: %v", err)
		return ""
	}
	return filepath.Join(home, ".kube", "config")
}

```

3. Event broadcaster

Event事件管理机制主要有三部分组成：

	* EventRecorder：是事件生成者，k8s组件通过调用它的方法来生成事件；
	* EventBroadcaster：事件广播器，负责消费EventRecorder产生的事件，然后分发给broadcasterWatcher；
	* broadcasterWatcher：用于定义事件的处理方式，如上报apiserver；

```golang
import utilruntime "k8s.io/apimachinery/pkg/util/runtime"

// 将自定义资源的scheme添加到kubernetes的scheme中用于logged events
utilruntime.Must(cnatscheme.AddToScheme(scheme.Scheme))
klog.V(4).Info("Creating event broadcaster")
// 创建事件广播器
eventBroadcaster := record.NewBroadcaster()
// 将收到的事件通过指定的log函数记录
eventBroadcaster.StartLogging(klog.Infof)
// 将收到的事件通过指定的Event Sink存储，相当于是broadcasterWatcher，这里将收到的事件创建成Events上报给API Server
eventBroadcaster.StartRecordingToSink(&typedcorev1.EventSinkImpl{Interface: kubeClientset.CoreV1().Events("")})
// 创建事件生产者
recorder := eventBroadcaster.NewRecorder(scheme.Scheme, corev1.EventSource{Component: controllerAgentName})
// 代码中就可以通过recorder来记录事件了
```

4. WorkQueue

WorkQueue称为工作队列，Kubernetes的WorkQueue队列与普通FIFO（先进先出，First-In, First-Out）队列相比，实现略显复杂，它的主要功能在于标记和去重，并支持如下特性。

	* 有序：按照添加顺序处理元素（item）。
	* 去重：相同元素在同一时间不会被重复处理，例如一个元素在处理之前被添加了多次，它只会被处理一次。
	* 并发性：多生产者和多消费者。
	* 标记机制：支持标记功能，标记一个元素是否被处理，也允许元素在处理时重新排队。
	* 通知机制：ShutDown方法通过信号量通知队列不再接收新的元素，并通知metric goroutine退出。
	* 延迟：支持延迟队列，延迟一段时间后再将元素存入队列。
	* 限速：支持限速队列，元素存入队列时进行速率限制。限制一个元素被重新排队（Reenqueued）的次数。
	* Metric：支持metric监控指标，可用于Prometheus监控。

WorkQueue支持3种队列，并提供了3种接口，不同队列实现可应对不同的使用场景，分别介绍如下。

	* Interface：FIFO队列接口，先进先出队列，并支持去重机制。
	* DelayingInterface：延迟队列接口，基于Interface接口封装，延迟一段时间后再将元素存入队列。
	* RateLimitingInterface：限速队列接口，基于DelayingInterface接口封装，支持元素存入队列时进行速率限制。

```golang
import "k8s.io/client-go/util/workqueue"

// k8s.io/client-go/util/workqueue/queue.go
type Interface interface {
    Add(item interface{})
    Len() int
    Get() (item interface{}, shutdown bool)
    Done(item interface{})
    ShutDown()
    ShuttingDown() bool
}

/*
Add：给队列添加元素（item），可以是任意类型元素。
Len：返回当前队列的长度。
Get：获取队列头部的一个元素。
Done：标记队列中该元素已被处理。
ShutDown：关闭队列。
ShuttingDown：查询队列是否正在关闭。
*/

// k8s.io/client-go/util/workqueue/rate_limiting_queue.go

// RateLimitingInterface is an interface that rate limits items being added to the queue.
type RateLimitingInterface interface {
	DelayingInterface

	// AddRateLimited adds an item to the workqueue after the rate limiter says it's ok
	AddRateLimited(item interface{})

	// Forget indicates that an item is finished being retried.  Doesn't matter whether it's for perm failing
	// or for success, we'll stop the rate limiter from tracking it.  This only clears the `rateLimiter`, you
	// still have to call `Done` on the queue.
	Forget(item interface{})

	// NumRequeues returns back how many times the item was requeued
	NumRequeues(item interface{}) int
}

/*
AddRateLimited: 将元素重新放回队列并进行限速
Forget：释放指定元素，清空该元素的排队数。
NumRequeues：获取指定元素的排队数。
*/
```


Ref:
1. https://kubernetes.io/zh/docs/concepts/workloads/controllers/garbage-collection/
2. https://www.kubernetes.org.cn/6839.html
3. https://www.cnblogs.com/luozhiyun/p/13799901.html
4. https://xie.infoq.cn/article/63258ead84821bc3e276de1f7


### Kubebuilder

1. Install

```shell
os=$(go env GOOS)
arch=$(go env GOARCH)

# download kubebuilder and extract it to tmp
curl -L https://go.kubebuilder.io/dl/2.3.1/${os}/${arch} | tar -xz -C /tmp/

# move to a long-term location and put it on your path
# (you'll need to set the KUBEBUILDER_ASSETS env var if you put it somewhere else)
sudo mv /tmp/kubebuilder_2.3.1_${os}_${arch} /usr/local/kubebuilder
export PATH=$PATH:/usr/local/kubebuilder/bin

# Also, you can install a master snapshot from 
https://go.kubebuilder.io/dl/latest/${os}/${arch}.
```

2. Create a Project

```shell
kubebuilder init --domain programming-kubernetes.info --license apache2 --owner "Programming Kubernetes authors”
```

3. Create API

```shell
kubebuilder create api --group cnat --version v1alpha1 --kind At
```
