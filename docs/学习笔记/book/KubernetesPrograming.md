
# Programming Kubernetes

## Chapter1. Introduction

* Kubernetes Native Application

何为Kubernetes-native的Application，感知是跑在Kubernetes上，并依靠Kubernetes提供的API(指的是与API Server直接交互来查询资源的状态或者更新这些资源的状态)来进行编程的Application，被称之为Kubernetes Native Application。


* Kubernetest 扩展系统
kubernetest提供了很强大的扩展系统，通常来说有多种方式来实现扩展，下面是一些常见的Kubernetes的扩展点，更多细节可以看[extending-kubernetes-101](https://speakerdeck.com/mhausenblas/extending-kubernetes-101):

	* `cloud-controller-manager` 对接各个云厂商提供的能力，比如Load Balancer、VM等
	* `Binary kubectl plug-ins` 通过二进制扩展kubelet子命令
	* `Binary kubelet plug-ins` 通过二进制扩展网络、存储、容器运行时等
	* `Access extensions in the API server` 比如dynamic admission control with webhooks
	* `Custom resources` 和 `custom controllers`
	* `Custom API servers`
	* Scheduler externsions，通过webhook来实现自己的调度器
	* Authentication with webhooks

* Controll Loop
Kubernetest的controller的实现本质上是一个control loop，通过API server来watch某种资源的状态，然后根据当前状态向着终态走。

> a controller implements a control loop, watching the shared state of the cluster through the API server and making changes in an attempt to move the current state toward the desired state
> Kubernetes并不会根据当前的状态和预期的状态来计算达到预期状态所需要的命令序列，从而来实现所谓的声明式系统，相反Kubernetes仅仅会根据当前的状态计算出下一个命令，如果没有可用的命令，则Kubernetes就达到稳态了

典型的Control loop的流程如下:

	1. 读取资源的状态(更可取的方式是通过事件驱动的方式来读取)
	2. 改变集群中对象的状态，比如启动一个POD、创建一个网络端点、查询一个cloud API等
	3. 通过API server来更新Setp1中的资源状态(Optimistic Concurrency)mak
	4. 循环重复，返回到Setp1

![controll-loop](../images/controll-loop.jpg)

Controller 核心数据结构:
	1. informers 提供一种扩展、可持续的方式来查看资源的状态，并实现了resync机制(强制执行定期对帐，通常用于确保群集状态和缓存在内存中的假定状态不会漂移)
	2. Work queues 用于将状态变化的事件进行排队处理，便于去实现重试(发生错误的时候，重新投入到队列中)


* Events

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

通过上面POD创建的过程可以看出，整个过程中有很多独立的Controller，每一个Controller就是一个control loop。
他们之间通过informer接收到事件来触发对应的逻辑。这些事件是API Server发送给informer的。informer内部通过watche的方式得到通知。

> 这里说的Event事件和Kubernetes中的Event对象是两回事，Event对象主要是给用户提供一种logging机制，用户编写的Controller可以创建Event对象来记录一些内部事件
> 比如kubelet会通过Event对象暴露内部的生命周期事件。这些Event对象可以像其他的kubernetes对象(Pod、Deployment等)一样进行查询。这些事件对象默认只存放
> 1个小时。1个小时候后便会从etcd中删除。


* Level triger vs Edge triger

Kubernetes中大量依赖事件来解耦各个组件，事件的高效通知对于Kubernetes来说至关重要，典型的二种实现事件通知的机制如下:

	* Edge-driven triggers
	At the point in time the state change occurs, a handler is triggered—for example, from no pod to pod running.

	* Level-driven triggers
	The state is checked at regular intervals and if certain conditions are met (for example, pod running), then a handler is triggered.

水平触发不具备可扩展性，本质上是polling、polling的间隔会影响controller的实时性，边缘触发更加高效，但是如果某个Controller存在bug就会导致事件丢失，这对于
边缘触发来说是无法接受的，而水平触发却不会存在这个问题，因为总是能够通过polling的方式获取到最终的状态。两者结合一下，事件通过边缘触发来通知，每次收到事件后通过pooling的方式
获取到资源的最终状态，那么即使中间丢失了一个事件也无所谓，比如replica set控制器中，预期要创建3个POD，因此每次POD创建都会产生一个事件，replica set通过事件就可以知道当前状态
和预期的状态还差多少，然后继续创建POD，如果因为网络问题导致中间丢失了一个事件，那么这就会导致创建的POD和预期的不符，这个时候如果结合水平触发，在下一次事件到来的时候主动查一下
当前的状态，这样就可以避免了中间事件丢失导致状态不对的问题，同时也借助了边缘触发达到了高效的事件通知。但是这样仍然存在问题。如果正好是最后一个事件丢失了呢? 这样就没有机会去查询当前
状态了。如果能够再结合定时查询就可以解决这个问题了，这个定时查询在Kubernetes中称之为resync。总结下，有三种事件通知策略：

1. Edge driven trigger 没有处理事件丢失的问题
2. Edge driven trigger + Level-driven triggers，总是去获取最新的状态(当有事件来的时候)，因此即使丢失一个事件，仍然可以通过获取最新的状态来进行业务逻辑
3. Edge driven trigger + Level-driven triggers + resync 如果最后一个事件丢失了，后面没有事件来了，所以也不会去触发(Level-driven triggers)，这个时候需要借助resync来得到最新的状态。

上面的这三种策略对应如下图:

![event driven trigger](../images/event-trigger.png)


kubernetes实现了上面的第三种策略，通过这种方式来实现高效的事件通知，如果你想知道更多关于水平触发以及reconcile请参考[level-triggering-and-reconciliation-in-kubernetes](https://hackernoon.com/level-triggering-and-reconciliation-in-kubernetes-1f17fe30333d)


* Optimistic Concurrency

在Controller的Control loop中会改变集群中对象的状态(比如创建一个POD)，然后将结果写到资源中的status中。实际中Controller通常会部署多个，因此这里更新资源的status字段是会存在并发写的。 

下图中描述了一种解决方案：

![optimistic-concurrent](../images/optimistic_concurrent.jpg)

这个解决方案是基于共享状态构建的新的并行调度器体系结构，使用无锁乐观并发控制，以实现可扩展性和性能可伸缩性。这种架构正在谷歌的下一代集群管理系统Omega中使用
Kubernetes大量参考了Omega。为了做了无锁并发写，Kubernetes也采用了乐观并发。这意味着当API Server探测到并发写(通过resource version来判断)，
它会拒绝掉后续的写操作。然后交由Controller自己来处理写入冲突的问题。可以简单的用下面的代码来表示这个过程。

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

乐观并发很适合Kubernetest中Controller的Controll Loop，Controll Loop中的水平触发总是获取到最新的状态，这个和乐观并发在失败后总是基于最新状态
再次发生写入的思想不谋而合。

> 写冲突错误在Controller中是完全正常的。我们应该总是预期它们会出现，并优雅地处理它们。
> client.Get返回的对象foo，包含了ObjectMeta字段，这个字段中包含了resource version，API Server借助这个字段来探测并发写。
> 边缘触发 + 水平触发 + resync + optimistic concurrency 是Kubernetes 事件驱动架构的核心

* Operators

一个SRE是一人，他来操作其他开发工程师写的软件，这个软件是具有领域知识的，因此要运维需要掌握这个软件的领域知识才能运维好。而这些运维所需要的领域知识称之为Operator。
一个Operator就是一个具有领域知识的用于运维的controller，借助了Kubernetes API进行扩展的Controller，借助这个Controller就可以实现简单的配置就达到运维复杂的带有状态的应用程序的效果。
一般来说，这个Controller通过一组具有领域知识的schema组成的crd来实现自动化运维。


***Reference**

* [extending-kubernetes-101](https://speakerdeck.com/mhausenblas/extending-kubernetes-101)
* [The Mechanics of Kubernetes](https://medium.com/@dominik.tornow/the-mechanics-of-kubernetes-ac8112eaa302)
* [A deep dive into Kubernetes controllers](https://engineering.bitnami.com/articles/a-deep-dive-into-kubernetes-controllers.html)
* [深入浅出Event Sourcing和CQRS](https://zhuanlan.zhihu.com/p/38968012)
* [Events, the DNA of Kubernetes](https://www.mgasch.com/post/k8sevents/)
* [QoS, "Node allocatable" and the Kubernetes Scheduler](https://www.mgasch.com/post/sched-reconcile/)
* [level-triggering-and-reconciliation-in-kubernetes](https://hackernoon.com/level-triggering-and-reconciliation-in-kubernetes-1f17fe30333d)
* [introducing-operators](https://coreos.com/blog/introducing-operators.html)


## Chapter2. Kubernetes API Basics

* API Server

API Server在Kubernetes中是一个核心组件，集群中所有的组件都是通过API Server来和底层的分布式存储etcd进行交互的。API Server的主要指责有几下几点:

1. 所有的组件通过API Server来解耦，通过API Server来产生事件和消费事件。
2. 负责对象的存储和读取，API Server最终还会和底层的etcd交互，将Kubernetes中的对象存储在etcd中。
3. API Server负责给集群内部的组件做代理，例如对Kubernetes dashboard、strea logs、service ports、以及kubectl exec等

API Server提供了符合RESTful类型的接口，主要是用于处理HTTP请求去查询和操作Kubernetes的资源，不同的HTTP method所代表的语义不同:

1. `Get` 获取到指定类型的资源，比如POD、或者是获取一个资源list，例如一个namespace下的所有POD
2. `POST` 创建一个资源，比如service、deployment等
3. `PUT` 更新一个已经存在的资源，比如改变一个POD中的容器镜像
4. `PATCH` 部分更新存在的资源，更多细节见: [Use a JSON merge patch to update a Deployment](https://kubernetes.io/docs/tasks/manage-kubernetes-objects/update-api-object-kubectl-patch/#use-a-json-merge-patch-to-update-a-deployment)
5. `DELETE` 销毁一个资源

> kubectl -n THENAMESPACE get pods 等同于 HTTP GET /api/v1/namespaces/THENAMESPACE/pods的结果。

* API Terminology

1. **Kind**: 

表示实体的类型，每一个对象都有一个Kind字段，kind主要有三类。
	
	1. 表示一个持久化的实体对象，比如Pod、Endpoints等
	2. 一个或多个kind实体，比如PodList、NodeLists等
	3. 特殊目的，比如binding、scale

2. **API Group**: 一堆Kind的逻辑上所属集合
3. **Version**: API group或者是对象的版本，一个group或对象可以存在多个版本。
4. **Resource**: 通常小写、复数形式(pods) 用来识别一系列的HTTP endpoints路径，用来暴露对象的CRUD语义，例如: `.../pods/nginx` 查看名为nginx的pod

> 所有的Resource都是具有CRUD语义的，但是也存在一些Resource可以支持更多的action，比如`.../pod/nginx/port-forward`，这个时候我们称之为`Subresources`。这需要通过自定义协议来替代RESET。例如exec是通过`WebSockets`来实现的。

在Kubernetes中，每一个Kind是直接映射到一个Golang类型的。

> Resources和Kind是相互的，Resources指定HTTP endpoints，而Kind是这个endpoints返回对象的类型，也是etcd中持久化的对象。
> 每一个对象都可以按照version v1来表示，也可以按照v1beta1来表示，可以返回不同的版本

`Resources`是API group和version的一部分，三者被称之为GVR(GroupVersionResource)，一个`GVR`唯一标示一个HTTP path。例如: `/apis/batch/v1/namespaces/default/jobs`
通过`GVR`可以获取到类型为kind的对象，同理这个对象也是属于这个version和Group的。因此称之为`GVK`(GroupVersionKind)

> 核心组zai /api/v1，命名组zai /apis/$name/$version，，为什么核心组不是/apis/core/v1呢? 这是因为历史原因导致的，API Group是核心组之后引入的。

例如下面这个http paths，就可以映射到一个`GVR`，通过`GVR`可以获取到类型为Kind的对象，也就间接的映射到了一个`GVK`。

![gvr](../images/gvr.jpg)

> GVK到GVR的映射在Kubernetes中被称之为`REST Mapping`。
> 除了GVR描述的HTTP path外，还存在另外一种类型的HTTP path，比如`/metrics`、`logs`、`healthz`等
> 通过在HTTP path后面添加`?watch=true`就可以watch到请求的资源，具体细节见: [watch modus](https://kubernetes.io/docs/reference/using-api/api-concepts/#efficient-detection-of-changes)

知道了HTTP path就可以通过curl访问API Server获取到资源，平时我们通过kubectl命令获取资源的方式内部其实也是通过访问HTTP path的方式来获取的，下面列举了两种通过HTTP path获取资源的方式。

```bash
kubectl proxy --port=8080
curl http://127.0.0.1:8080/apis/batch/v1

或

kubectl get --raw /apis/batch/v1
```

* API Server如何处理请求

1. 首先HTTP request会被`DefaultBuildHandlerChain`注册的filters chain来处理(鉴权、admission、validation等)
2. 接着根据HTTP path走到分发器，通过分发器来路由到最终的handler
3. 每一个gvr都会注册一个handler

![apiserver process](../images/apiserver-process.jpg)


***Reference**

* [Use a JSON merge patch to update a Deployment](https://kubernetes.io/docs/tasks/manage-kubernetes-objects/update-api-object-kubectl-patch/#use-a-json-merge-patch-to-update-a-deployment)
* [watch modus](https://kubernetes.io/docs/reference/using-api/api-concepts/#efficient-detection-of-changes)


## Chapter3. Basics of client-go

### `client-go`、`api`、`apimachinery`三个重要的仓库

`client-go`、`api`、`apimachinery`是Kubernetes client中最核心的三个仓库。

1. [client-go](https://github.com/kubernetes/client-go)仓库是用来访问kubernetes的client的接口。
2. Pod、Deployment等对象则是放在[api](https://github.com/kubernetes/api)的仓库中，例如Pod对象，它属于core group，对于v1版本来说，它的位置就在`api/core/v1`目录下。
   Pod的类型定义就在`types.go`文件中。这个目录下还包含了一些其他文件，部分文件都是通过代码生成器自动生成的。
3. 最后一个仓库是[apimachinery](https://github.com/kubernetes/apimachinery)，包含了所有通用的用来构建类似Kubernetes风格API的模块。

### Creating and Using a Client

```golang
package main

import (
	"fmt"
	"os"
	"path/filepath"
	"time"

	"k8s.io/apimachinery/pkg/util/wait"
	"k8s.io/client-go/informers"
	"k8s.io/client-go/rest"

	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/tools/cache"
	"k8s.io/client-go/tools/clientcmd"
)

func main() {
	// 先从集群Pod中/var/run/secrets/kubernetes.io/serviceaccount获取到service account转换为rest.Config
	config, err := rest.InClusterConfig()
	if err != nil {
		// 获取service account失败，直接去读取kubeconfig文件
		kubeconfig := filepath.Join("~", ".kube", "config")
		// 或者是从环境变量中获取到kubeconfig的位置
		if envvar := os.Getenv("KUBECONFIG"); len(envvar) > 0 {
			kubeconfig = envvar
		}
		// 通过kubeconfig文件构建rest.Config
		config, err = clientcmd.BuildConfigFromFlags("", kubeconfig)
		if err != nil {
			fmt.Printf("The kubeconfig cannot be loaded: %v\n", err)
			os.Exit(1)
		}
		// 返回的config可以做一些自定义操作，比如自定义UserAgent、自定义AcceptContentTypes、超时实际、限流等
		// config.UserAgent = fmt.Sprintf("Go %s", runtime.GOOS);
		// config.AcceptContentTypes = "application/vnd.kubernetes.protobuf,application/json"
	}
	// 用reset.Config构建kubernetes client
	clientset, err := kubernetes.NewForConfig(config)
	// 读取book namespace下的名为example的Pod对象
	pod, err = “clientset.CoreV1().Pods("book").Get("example", metav1.GetOptions{})
}
```


### Versioning和Compatibillity

Kubernetes API是带有版本的，每个对象都有不同的版本，我们可以在api仓库中的`apps`目录下可以看到各个版本的对象存在，同样的，对于client-go来说，针对不同的对象也存在不同的版本的接口。我们可以在
client-go仓库下的`kubernestes/typed/apps`目录下找到对应版本对象的接口。Kubernestes和`client-go`是共用相同的api仓库的，因此client-go的版本需要和kubernetes具有兼容的版本才能发挥作用，
否则Api Server会拒绝掉`client-go`发出来的请求。如果client-go的版本比kubernertes的要新，那么当携带某些新增字段的时候，kubernetes可能会拒绝掉，也有可能会忽略掉，这个要看具体的字段的行为。
kubernetes为了解决对象版本兼容问题，在实际将对象存储在etcd中时会按照一个称之为内部版本的对象存储进去，不同版本的API请求过来的时候，通过预定义的转换器进行转换来实现版本之间的兼容。

### Kubernetes Objects in Go

Kubernetes中的资源，准确来说对应到Go中就是一个对象，资源的类型对应到yaml中的Kind字段，比如下面这个Pod资源。其yaml中的Kind字段就是Pod。
在Kubernetest中会通过一个`struct`来表示这个Pod，我们还可以发现的Kubernetes中所有的资源都会有一些公共的字段，比如apiVersion、Kind、metadata、spec等。

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: pod-demo
  namespace: default
  labels:
    app: myapp
    tier: fronted
spec:
  containers:
  - name: myapp
    image: ikubernetes/myapp:v1
  - name: busybox
    image: busybox:latest
    command:
    - "bin/sh"
    - "-c"
    - "echo $(date) >> /tmp/txt; sleep 5"
```

Kubernetes中资源所对应的对象都默认实现了`runtime.Object`接口，这个接口很简单，定义如下。

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

例如一个POD对象如下:

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

每一个对象都有自己独立的Spec和Status，通常Spec表示用户的期望，Status则是期望的的结果，是Controller和Operator来负责填充。也存在一些异常情况，比如endpoints和RBAC


> 请求分为长请求和短请求，对于长请求一般来说是诸如watch、一些Subresources(exec、sport-forward)等，对于短请求则会有60s的超时时间，当API server下线的时候会等待60s直到服务完这些短请求
> 对于长请求则直接断掉。从而实现所谓的优雅关闭。

### Client Sets

在上面的example中，通过`kubernetes.NewForConfig(config)`创建了一个`Clientset`，一个`Clientset`是可以用来访问多个`API Group`和资源的接口，其定义如下。


```go
// Clientset contains the clients for groups. Each group has exactly one
// version included in a Clientset.
type Clientset struct {
	*discovery.DiscoveryClient
	admissionregistrationV1      *admissionregistrationv1.AdmissionregistrationV1Client
	admissionregistrationV1beta1 *admissionregistrationv1beta1.AdmissionregistrationV1beta1Client
	appsV1                       *appsv1.AppsV1Client
	appsV1beta1                  *appsv1beta1.AppsV1beta1Client
	appsV1beta2                  *appsv1beta2.AppsV1beta2Client
	auditregistrationV1alpha1    *auditregistrationv1alpha1.AuditregistrationV1alpha1Client
	authenticationV1             *authenticationv1.AuthenticationV1Client
	authenticationV1beta1        *authenticationv1beta1.AuthenticationV1beta1Client
	authorizationV1              *authorizationv1.AuthorizationV1Client
	authorizationV1beta1         *authorizationv1beta1.AuthorizationV1beta1Client
	autoscalingV1                *autoscalingv1.AutoscalingV1Client
	autoscalingV2beta1           *autoscalingv2beta1.AutoscalingV2beta1Client
	autoscalingV2beta2           *autoscalingv2beta2.AutoscalingV2beta2Client
	batchV1                      *batchv1.BatchV1Client
	batchV1beta1                 *batchv1beta1.BatchV1beta1Client
	batchV2alpha1                *batchv2alpha1.BatchV2alpha1Client
	certificatesV1beta1          *certificatesv1beta1.CertificatesV1beta1Client
	coordinationV1beta1          *coordinationv1beta1.CoordinationV1beta1Client
	coordinationV1               *coordinationv1.CoordinationV1Client
	coreV1                       *corev1.CoreV1Client
	discoveryV1alpha1            *discoveryv1alpha1.DiscoveryV1alpha1Client
	discoveryV1beta1             *discoveryv1beta1.DiscoveryV1beta1Client
	eventsV1beta1                *eventsv1beta1.EventsV1beta1Client
	extensionsV1beta1            *extensionsv1beta1.ExtensionsV1beta1Client
	flowcontrolV1alpha1          *flowcontrolv1alpha1.FlowcontrolV1alpha1Client
	networkingV1                 *networkingv1.NetworkingV1Client
	networkingV1beta1            *networkingv1beta1.NetworkingV1beta1Client
	nodeV1alpha1                 *nodev1alpha1.NodeV1alpha1Client
	nodeV1beta1                  *nodev1beta1.NodeV1beta1Client
	policyV1beta1                *policyv1beta1.PolicyV1beta1Client
	rbacV1                       *rbacv1.RbacV1Client
	rbacV1beta1                  *rbacv1beta1.RbacV1beta1Client
	rbacV1alpha1                 *rbacv1alpha1.RbacV1alpha1Client
	schedulingV1alpha1           *schedulingv1alpha1.SchedulingV1alpha1Client
	schedulingV1beta1            *schedulingv1beta1.SchedulingV1beta1Client
	schedulingV1                 *schedulingv1.SchedulingV1Client
	settingsV1alpha1             *settingsv1alpha1.SettingsV1alpha1Client
	storageV1beta1               *storagev1beta1.StorageV1beta1Client
	storageV1                    *storagev1.StorageV1Client
	storageV1alpha1              *storagev1alpha1.StorageV1alpha1Client
}
```

比如通过`Clientset`的appsV1接口，就可以访问apps组，v1 version下的所有资源，在这个组下有DaemonSet、ControllerRevision、Deployment、ReplcaSets、StatefulSet等资源，appsV1定义如下:

```go
// DeploymentsGetter has a method to return a DeploymentInterface.
// A group's client should implement this interface.
type DeploymentsGetter interface {
	Deployments(namespace string) DeploymentInterface
}

type AppsV1Interface interface {
	RESTClient() rest.Interface
	ControllerRevisionsGetter
	DaemonSetsGetter
	DeploymentsGetter
	ReplicaSetsGetter
	StatefulSetsGetter
}

// AppsV1Client is used to interact with features provided by the apps group.
type AppsV1Client struct {
	restClient rest.Interface
}

// DeploymentInterface has methods to work with Deployment resources.
type DeploymentInterface interface {
	Create(*v1.Deployment) (*v1.Deployment, error)
	Update(*v1.Deployment) (*v1.Deployment, error)
	UpdateStatus(*v1.Deployment) (*v1.Deployment, error)
	Delete(name string, options *metav1.DeleteOptions) error
	DeleteCollection(options *metav1.DeleteOptions, listOptions metav1.ListOptions) error
	Get(name string, options metav1.GetOptions) (*v1.Deployment, error)
	List(opts metav1.ListOptions) (*v1.DeploymentList, error)
	Watch(opts metav1.ListOptions) (watch.Interface, error)
	Patch(name string, pt types.PatchType, data []byte, subresources ...string) (result *v1.Deployment, err error)
	GetScale(deploymentName string, options metav1.GetOptions) (*autoscalingv1.Scale, error)
	UpdateScale(deploymentName string, scale *autoscalingv1.Scale) (*autoscalingv1.Scale, error)

	DeploymentExpansion
}

// Get takes name of the deployment, and returns the corresponding deployment object, and an error if there is any.
func (c *deployments) Get(name string, options metav1.GetOptions) (result *v1.Deployment, err error) {
	result = &v1.Deployment{}
	err = c.client.Get().
		Namespace(c.ns).
		Resource("deployments").
		Name(name).
		VersionedParams(&options, scheme.ParameterCodec).
		Do().
		Into(result)
	return
}
```

AppsV1Client实现了AppsV1Interface接口，通过这个接口可以访问这个组下的所有资源，通过其定义可以看出，最终都是通过rest接口来访问的。
注意观察你会发现上面的接口中都带有一个Options，比如ListOptions、DeleteOptions、GetOptions等，通过这些Options可以自定义一些过滤条件，比如ListOptions中可以指定label selector进行过滤。
另外上面的接口中还有一个Watch接口，这个接口是用来监听对象的所有改变(添加、删除、更新)，返回的watche.Interface其定义如下。

```go
// Interface can be implemented by anything that knows how to watch and report changes.
type Interface interface {
	// Stops watching. Will close the channel returned by ResultChan(). Releases
	// any resources used by the watch.
	Stop()

	// Returns a chan which will receive all the events. If an error occurs
	// or Stop() is called, this channel will be closed, in which case the
	// watch should be completely cleaned up.
	ResultChan() <-chan Event
}

// EventType defines the possible types of events.
type EventType string

const (
	Added    EventType = "ADDED"
	Modified EventType = "MODIFIED"
	Deleted  EventType = "DELETED"
	Bookmark EventType = "BOOKMARK"
	Error    EventType = "ERROR"

	DefaultChanSize int32 = 100
)

// Event represents a single event to a watched resource.
// +k8s:deepcopy-gen=true
type Event struct {
	Type EventType

	// Object is:
	//  * If Type is Added or Modified: the new state of the object.
	//  * If Type is Deleted: the state of the object immediately before deletion.
	//  * If Type is Bookmark: the object (instance of a type being watched) where
	//    only ResourceVersion field is set. On successful restart of watch from a
	//    bookmark resourceVersion, client is guaranteed to not get repeat event
	//    nor miss any events.
	//  * If Type is Error: *api.Status is recommended; other types may make sense
	//    depending on context.
	Object runtime.Object
}
```

> 不鼓励直接使用watch接口，应该使用封装好的Informes。


### Informers and Caching

Informers通过watch接口实现Cachae和增量更新。并能够很好的处理网络抖动，断网等场景。尽可能的每一种资源类型只创建一个Informers，否则会导致资源的浪费，为此可以通过`InformerFactory`来创建Informer。
他内部对于同一个资源类型只会创建一个informer实例。

> It is very important to remember that any object passed from the listers to the event handlers is owned by the informers. If you mutate it in any way, 
> you risk introducing hard-to-debug cache coherency issues into your application. Always do a deep copy (see “Kubernetes Objects in Go”) before changing an object.

我们在informers的event回调中切记不要修改对象，否则会导致很难排查的缓存一致性问题，如果要修改的话，请先深拷贝，然后修改。

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

![informers](images/informers.jpg)

### Object Owner

通常来说，在修改一个对象之前，我们总是会问自己，这个对象被谁拥有，或者是在哪个数据结构中? 一般来来说原则如下:

1. Informers and listers拥有他们返回的对象，要修改这个对象的时候，需要进行深拷贝
2. Clients返回的新对象这个属于调用者
3. Conversions返回的共享对象，如果调用者拥有输入的对象，那么它不拥有输出的共享对象。

### API Machinery in Depth

API Machinery仓库实现了基本的Kubernetes类型系统，但是类型系统是什么呢?  类型这个术语并不在API Machinery仓库中存在。
在API Machinery类型对应到的是Kinds。

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


### Scheme
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



## Chapter4. Using Custom Resources

CRD本身是一个Kubernetes的资源，它描述了在集群中可用的资源，典型的一个CRD定义如下：

```yaml
apiVersion: apiextensions.k8s.io/v1beta1
kind: CustomResourceDefinition
metadata:
  name: ats.cnat.programming-kubernetes.info
spec:
  group: cnat.programming-kubernetes.info
  names:
    kind: At
    listKind: AtList
    plural: ats
    singular: at
  scope: Namespaced
  subresources:
	status: {}
	
version: v1alpha1
versions:
- name: v1alpha1
  served: true
  storage: true
```

注意，这个CRD的名字需要资源名的复数形式，然后跟上API group name，上面的CRD中资源名为at，API Group的名字就是`cnat.programming-kubernetes.info`
定义完这个CRD后，我们就可以创建一个at资源了。然后通过`kubectl get ats`就可以列出所有创建的at资源。

```yaml
apiVersion: cnat.programming-kubernetes.info/v1alpha1
kind: At
metadata:
  name: example-at
spec:
  schedule: "2020-12-02T00:30:00Z"
  containers:
  - name: shell
    image: centos:7
    command:
    - "bin/bash"
    - "-c"
    - echo "Kubernetes native rocks!"
status:
  phase: "pending"
```

kubectl是如何找到`ats`资源呢?


* 如何找到自定义资源
1. 通过/apis询问Api server所有的 API group
2. 通过/apis/group/version 查看所有的group存在的资源，找到对应资源所在的Group、VersionheResources


## Subresources

Subresources就是一个特殊的HTTP endpoints，一般是在正常resource后面添加的一个后缀来表示，比如，对于pod资源来说，正常的HTTP Path是
 `/api/v1/namespace/namespace/pods/name`，他可以有多个Subresources，比如`/logs`、`/portforward`、`/exec`、`status`。

Subresources所使用的协议是和主资源不一样的，目前为止自定义资源支持/scale和/status两种子资源。

/status子资源的目的是为了和将spec和status进行分离，

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


5. Scheme

```golang

import "k8s.io/apimachinery/pkg/runtime/schema"
import metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

// SchemeGroupVersion is group version used to register these objects
var SchemeGroupVersion = schema.GroupVersion{Group: GroupName, Version: "v1alpha1"}

// Adds the list of known types to Scheme.
// AddKnownTypes方法中会通过反射获取到资源对象的名字，然后和GroupVersion组合成GVK，最后用GVK和对象建立映射关系。
func addKnownTypes(scheme *runtime.Scheme) error {
	scheme.AddKnownTypes(SchemeGroupVersion,
		&At{},
		&AtList{},
	)
	// 构建Scheme管理多version
	metav1.AddToGroupVersion(scheme, SchemeGroupVersion)
	return nil
}

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
```

6. Finalizers

Finalizers 字段属于 Kubernetes GC 垃圾收集器，是一种删除拦截机制，能够让控制器实现异步的删除前（Pre-delete）回调。
其存在于任何一个资源对象的 Meta 中，在 k8s 源码中声明为 []string，该 Slice 的内容为需要执行的拦截器名称。

The key point to note is that a finalizer causes “delete” on the object to become an “update” to set deletion timestamp. 

finalizer会导致对象的删除变成对象的deletion timestamp字段的更新。

```golang
unc (r *CronJobReconciler) Reconcile(req ctrl.Request) (ctrl.Result, error) {
    ctx := context.Background()
    log := r.Log.WithValues("cronjob", req.NamespacedName)

    var cronJob *batchv1.CronJob
    if err := r.Get(ctx, req.NamespacedName, cronJob); err != nil {
        log.Error(err, "unable to fetch CronJob")
        // we'll ignore not-found errors, since they can't be fixed by an immediate
        // requeue (we'll need to wait for a new notification), and we can get them
        // on deleted requests.
        return ctrl.Result{}, client.IgnoreNotFound(err)
    }

    // name of our custom finalizer
    myFinalizerName := "storage.finalizers.tutorial.kubebuilder.io"

    // examine DeletionTimestamp to determine if object is under deletion
    if cronJob.ObjectMeta.DeletionTimestamp.IsZero() {
        // The object is not being deleted, so if it does not have our finalizer,
        // then lets add the finalizer and update the object. This is equivalent
		// registering our finalizer.
		// 注册finalizer
        if !containsString(cronJob.ObjectMeta.Finalizers, myFinalizerName) {
            cronJob.ObjectMeta.Finalizers = append(cronJob.ObjectMeta.Finalizers, myFinalizerName)
            if err := r.Update(context.Background(), cronJob); err != nil {
                return ctrl.Result{}, err
            }
        }
    } else {
        // The object is being deleted
        if containsString(cronJob.ObjectMeta.Finalizers, myFinalizerName) {
            // our finalizer is present, so lets handle any external dependency
            if err := r.deleteExternalResources(cronJob); err != nil {
                // if fail to delete the external dependency here, return with error
                // so that it can be retried
                return ctrl.Result{}, err
            }

            // remove our finalizer from the list and update it.
            cronJob.ObjectMeta.Finalizers = removeString(cronJob.ObjectMeta.Finalizers, myFinalizerName)
            if err := r.Update(context.Background(), cronJob); err != nil {
                return ctrl.Result{}, err
            }
        }

        // Stop reconciliation as the item is being deleted
        return ctrl.Result{}, nil
    }

    // Your reconcile logic

    return ctrl.Result{}, nil
}

func (r *Reconciler) deleteExternalResources(cronJob *batch.CronJob) error {
    //
    // delete any external resources associated with the cronJob
    //
    // Ensure that delete implementation is idempotent and safe to invoke
    // multiple types for same object.
}

// Helper functions to check and remove string from a slice of strings.
func containsString(slice []string, s string) bool {
    for _, item := range slice {
        if item == s {
            return true
        }
    }
    return false
}

func removeString(slice []string, s string) (result []string) {
    for _, item := range slice {
        if item == s {
            continue
        }
        result = append(result, item)
    }
    return
}
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

4. API Interface

```shell
// AtReconciler reconciles a At object
type AtReconciler struct {
	client.Client
	Log    logr.Logger
	Scheme *runtime.Scheme
}

// +kubebuilder:rbac:groups=cnat.programming-kubernetes.info,resources=ats,verbs=get;list;watch;create;update;patch;delete
// +kubebuilder:rbac:groups=cnat.programming-kubernetes.info,resources=ats/status,verbs=get;update;patch

// 核心的Reconciler接口，通过client.Client可以访问自定义资源和k8s基本资源
func (r *AtReconciler) Reconcile(req ctrl.Request) (ctrl.Result, error) {
	_ = context.Background()
	_ = r.Log.WithValues("at", req.NamespacedName)

	// your logic here

	return ctrl.Result{}, nil
}
```

### Operator-SDK
等同于kubebuilder

## Shipping Controller and Operator

### Packageing

通过helm来渲染YAML文件，解决YAML文件只能是静态的问题，这样就可以动态控制YAML文件了。
还可以通过kustomize


never use the default service account in a namespace


## Custom API Server

CRD的一些限制:

1. 限制只能使用etcd作为存储
2. 不支持protobuf，只能是JSON
3. 只支持/status和/scale两种子资源
4. 不支持graceful deletetion、尽管可以通过Finalizer来模拟，但是不支持指定graceful deletion time
5. 对API Server的负载影响比较大，因为需要用通用的方式来实现所有正常资源需要走的逻辑和算法
6. 只能实现CRUD基本语义
7. 不支持同类资源的存储共享(比如不同API Group的相同资源底层不支持使用相同的存储)

相反一个自定义的API Server没有上面的限制。

1. 可以使用任何存储，例如metrics API Server可以存储数据在内存中
2. 可以提供protobuf支持
3. 可以提供任意的子资源
4. 可以实现graceful deletion
5. 可以实现所有的操作
6. 可以实现自定义语义，比如原子的分配ip，如果使用webbook的方式可能会因为后续的pipeline导致请求失败，这个时候分配的ip需要取消，但是webhook是没办法做撤销的，需要结合控制器来完成。这就是因为
webhook可能会产生副作用。
7. 可以对底层类型相同的资源，进行共享存储。

自定义API Server工作流程:
1. K8s API server接收到请求
2. 请求传递了handler chanin，这里面包含了鉴权、日志审计等
3. 请求会走到kube-aggregator组件，这个组件知道哪些API 请求是需要走自定义API Server的，那些Group走API server这是API Service定义的
4. 转发请求给自定义API Server

> 自定义API Server的鉴权可以delegated给k8s的 API Server，通过SubjectAccessReview来实现

```yaml

```

// 定义哪些group、version的资源要走自定义Api Server
```yaml
apiVersion: apiregistration.k8s.io/v1beta1
kind: APIService
metadata:
  name: name
spec:
  group: API-group-name
  version: API-group-version
  service:
    namespace: custom-API-server-service-namespace
    name: -API-server-service
  caBundle: base64-caBundle
  insecureSkipTLSVerify: bool
  // 相同的group高优先级覆盖低优先级
  groupPriorityMinimum: 2000
  // 相同group的不同version通过优先级来选择
  versionPriority: 20
```



Every API server serves a number of resources and versions 
Some resources have multiple versions. To make multiple versions of a resource possible, the API server converts between versions.
To avoid quadratic growth of necessary conversions between versions, API servers use an internal version when implementing the actual API logic. 
The internal version is also often called hub version because it is a kind of hub that every other version is converted to and from

API Server在内部给每一个资源都维护了一个内部版本，所有的版本都会转换成这个内部版本再去操作。

1. 用户发送指定版本的请求给API server(比如v1)
2. API server解码请求，然后转换为内部版本
3. API server传递内部版本给admission 和 validation
4. API server在registry中实现的逻辑是根据内部版本来实现的
5. etcd读和写带有版本的对象(例如v2，存储版本)，他将从内部版本进行转换。
6. 最终结果会将转换为请求的版本，比如这里就是v1


Default和Conversion需要给内部版本和外部版本提供Conversion方法和默认值。

This trick of using a pointer works for primitive types like strings. For maps and arrays, it is often hard to reach roundtrippability without identifying nil maps/arrays and empty maps/arrays.
Most defaulters for maps and arrays in Kubernetes therefore apply the default in both cases, working around encoding and decoding bugs.

对于基本类型如何区分默认的零值是设置了还是没有设置，比如bool默认是false，那用户到底是设置了false、还是没有设置导致默认值用了false呢? k8s通过指针来解决，如果有设置那么指针不为空，否则就是没有设置。



TODO(tianqian.zyf): 实现一个Custom API Server


## Advanced Custom Resources

versioning、coversion、admission controllers

通过versioning机制可以保证API的演进，同时也可以向后兼容，versiong机制的核心在于Conversion。

```yaml
apiVersion: apiextensions.k8s.io/v1beta1
kind: CustomResourceDefinition
metadata:
  name: pizzas.restaurant.programming-kubernetes.info
spec:
  group: restaurant.programming-kubernetes.info
  names:
    kind: Pizza
    listKind: PizzaList
    plural: pizzas
    singular: pizza
  scope: Namespaced
  version: v1alpha1
  versions:
  // 定义v1alpha1为存储版本，
  - name: v1alpha1
    served: true
    storage: true
    schema: ...
  - name: v1beta1
    served: true
    storage: false
    schema: ...
```

1. The client (e.g., our kubectl get pizza margherita) requests a version.
2. etcd has stored the object in some version.
3. If the versions do not match, the storage object is sent to the webhook server for conversion. The webhook returns a response with the converted object.
4. The converted object is sent back to the client.

```go
type ConversionReview struct {
    metav1.TypeMeta `json:",inline"`
    Request *ConversionRequest
    Response *ConversionResponse
}

```