# Helm


## Chart

一个Chart的目录结构

```yaml
mychart/
  Chart.yaml
  values.yaml
  charts/
  templates/
  ...
```



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

kubernetes实现了上面的第三种策略


## API

“In Kubernetes programs, a kind directly corresponds with a Golang type.”

Kind: 实体的类型
API Group: 一堆kind的逻辑上所属集合
Version: 每一个Kind的版本 (并不是每一个版本都有一个对象存在，而是一个对象可以表示不同的版本)
Resource: 通常小写、复数形式(pods) 用来识别一系列的HTTP endpoints路径，用来暴露对象的CRUD语义

![gvr](images/gvr.jpg)


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

可以看出，每一个Kubernetes中的对象，都可以设置和获取GroupVerisonKind(GVK)，以及进行深度拷贝。

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
```

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

每一个对象都有自己独立的Spec和Status


## Glang type、GroupVersionKind、GroupVerisonResource、HTTP path



## object owner

“In general: before mutating an object, always ask yourself who owns this object or the data structures in it. As a rule of thumb:”

1. “Informers and listers own objects they return. Hence, consumers have to deep-copy before mutation.”
2. “Clients return fresh objects, which the caller owns.”
3. “Conversions return shared objects. If the caller does own the input object, it does not own the output.”


## CustomResourceDefinition

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
		panic(fmt.Sprintf("Double registration of different types for %v: old=%v.%v, new=%v.%v in scheme %q", gvk, oldT.PkgPath(), oldT.Name(), t.PkgPath(), t.Name(), s.schemeName))
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