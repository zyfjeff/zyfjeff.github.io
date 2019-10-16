## Kubeadm

1. 安装docker, systemctl starte docker.service，设置HTTPS_PROXY、NO_PROXY
2. 安装kubernetes yum源

```
cat <<EOF > /etc/yum.repos.d/kubernetes.repo
[kubernetes]
name=Kubernetes
baseurl=https://mirrors.aliyun.com/kubernetes/yum/repos/kubernetes-el7-x86_64/
enabled=1
gpgcheck=1
```
3. 安装kubelet kubectl kubeadm等
4. 编辑 /etc/sysconfig/kubelet 设置其忽略Swap启用的错误

```
KUBELET_EXTRA_ARGS="--fail-swap-on=false"
KUBE_PROXY_MODE=ipvs
```

5. 设置docker和kubelet开机自启动

```
kubectl enable docker kubelet
```
6. kubeadm init --ignore-preflight-errors=Swap --pod-network-cidr=xxxx service-cidr=xxxx
7. 参考步骤6将其他节点加入


## Basic command

* kubectl run

  * easy way to get started
  * versatile

* `kubectl create <resource>`

  * explicit, but lacks some features
  * can't create a CronJob
  * can't pass command-line arguments to deployments

* `kubectl create -f foo.yaml` or `kubectl apply -f foo.yaml`

  * all features are available
  * requires writing YAML

```bash
kubectl run --generator=run-pod/v1 nginx --image=nginx 仅仅创建出pod容器
kubectl run nginx --image=nginx 创建pod容器的同时，会创建一个名为nginx的deployment
```

--grace-period/--now 覆盖默认的宽限期
--cascade=false 默认删除一种资源，会将其关联的其他资源都删除，通过这个选项可以让其不删除其关联的资源


## 资源对象
1. workload(工作负载型资源): Pod、ReplicaSet、Deplovment、StatefulSet、DaemonSet、Job、Cronjob
2. 服务发现和均衡: Service、Ingress
3. 配置与存储: Volume、CSI、ConfigMap、Secret、DownwardAPI
4. 集群级资源: Namespace、Node、Role、ClusterRole、RoleBinding、ClusterRoleBinding
5. 元数据型资源: HPA、PodTemplate、LimitRange

核心目标是为了更好的运行和丰富Pod资源

## 资源定义
ApiServer仅接收JSON格式的资源定义:(yaml更易读，可以无损转化为JSON)
大部分资源的配置清单都由五个部分组成:
1. apiVersion: group/version (kubectl api-versions，省略掉组的话就是core类型的核心组)
2. kind: 资源类型，分为对象类(Pod、Namespace、Depolyment等)、列表类(PodList、NamespaceList)、简单类
3. metadata:
	1. name
	2. namespace
	3. labels
	4. annotations
	5. selflink: /api/GROUP/VERSION/namespaces/NAMESPACE/TYPE/NAME
4. spec: 定义期望的状态，disired state (kubectl explain pods)
5. status:  当前状态，current state，本字段由kubernetes集群维护


通过kubectl explain TYPE来查看资源定义的描述，可以嵌套查询kubectl explan TYPE.spec.xxx

Example:

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

> attach到容器中 kubectl exec -it POD -c CONTAINER -- cmd



## Pod资源
  sepc.containers: <[]Object>
  - name <string>
    image <string>
    imagePullPolicy <string> (Always(总是下载)、Never(总是使用本地)、IfNotPresent(如果本地有就使用，否则就远程拉)) 如果标签是latest就是Always，否则就是IfNotPresent (不能动态更新)
    lifecycle:
      postStart:  // 容器启动后(ENTRYPOINT执行之后)立刻执行的动作，是异步的，容器继续启动
        exec:
          command: [xxxx]
      preStop:    // 容器被杀死之前，是同步的，会阻塞当前容器杀死的流程
        exec:
          command: [xxxx]
    ports: <[]Object> 容器暴露的端口，只是信息暴露，通过yaml知道这个应用暴露了哪些端口
      containerPort:
      hostIP:
      hostPort:
      name:
      protocol: 默认TCP
    args: <[]string> 向entrypoint传递参数，代替镜像中的CMD参数
    command: <[]string> 相当于entrypoint，是不会运行在shell中的，如果不提供就运行镜像中的entrypoint
  spec <[]Object>
  nodeSelector: <map[string]string> 选择要运行在哪类node上
  nodeName: <string> 指定要运行的node机器上
  hostPID:
  hostIPC:
  hostAliases:  // 定义pod中的hosts文件里的内容
    - ip: "10.1.2.3"
      hostnames:
      - "foo.remote"
      - "bar.remote"
  shareProcessNamespace: true //共享PID namespace
  hostNetwork:
  restartPolicy: Always(失败后会不断重新创建pod)、OnFailure(启动失败后，会不断重启pod中的容器)、Never、Default to Always
  env:
  -name:
   value:
  metadata:
    annotations: <map[string]string> 类似labels，不能用于挑选资源对象，仅用于为对象提供"元数据"

Pod生命周期:
  1. 串行执行多个init初始化容器
  2. 主容器运行
  3. post start，主容器运行后可以执行的hook
  4. pre stop 主容器退出前可以执行的hook
  5. 存活状态检测 liveness probe
  6. 服务可用状态检测 readiess probe


> POD并不支持原地更新，需要有Deployment来支持

状态:
  1. Pending  // YAML文件提交给k8s饿，API对象已经创建并保存在Etcd中，但是Pod里面有些容器因为某种原因不能被顺利创建。
  2. Running  // Pod已经调度成功，跟一个具体的节点绑定，它包含的容器都已经创建成功了，并且至少有一个正在运行中
  3. Failed   // Pod里面至少有一个容器以不正常的状态(非0的返回码)退出，这个状态的出现，意味着你得想办法Debug这个容器的应用。
  4. Succeeded // Pod里面所有的容器都正常运行完毕了，并且已经退出了。
  5. Unknown  // 一个异常的状态，Pod的状态不能持续被kubelet汇报给kube-apiserver，可能是主节点master和kubelet间的通信出现了问题

进一步又可以细分:
PodScheduled、Ready、Initialized，以及 Unschedulable，用来描述造成当前Status的具体原因是什么?

> 1. 只要Pod的restartPolicy指定的策略允许重启异常的容器(比如: Always)，那么这个Pod就会保持Running状态，并进行容器重启，否则就会进程Failed状态
> 2. 对于包含多个容器的Pod，只有它里面所有的容器都进程异常状态后，Pod才会进程Failed状态，在此之前Pod都是Running状态。此时Pod的READY字段会显示正常容器的个数

```
Image Entrypoint|Image Cmd|Container command|Container args|Command run
  [/ep-1]        [foo bar]     <not set>       <not set>    [ep-1 foo bar]
  [/ep-1]        [foo bar]      [/ep-2]        <not set>    [ep-2]
  [/ep-1]        [foo bar]      <not set>      [zoo boo]    [ep-1 zoo boo]
  [/ep-1]        [foo bar]      [/ep-2]        [zoo boo]    [ep-2 zoo boo]
```

> kubectl get pods --show-labels 显示资源的labels
> kubectl get pods -L app 显示资源对象的时候，显示具有app标签的标签值
> kubectl get pods -l app 只显示含有app标签的资源
> kubectl label TYPE NAME LABEL，给TYPE类型的资源中名为NAME的资源进行打标

标签选择器:
1. 等值关系 =, ==, !=
2. 集合关系
  1. KEY in (VALUE...)
  2. KEY notin (VALUE...)
  3. KEY
  4. !KEY

许多资源支持内嵌字段定义其使用的标签选择器:
1. matchLabels
2. matchExpressions

探针类型(livenessProbe、readynessProb、lifecyle):
1. ExecAction
2. TcpSocketAction
3. HTTPGetAction

> lifecyle含有postStart和preStop两个hook

Projected Volume 投射数据卷，是为容器提供的预先定义好的数据

1. Secret                 // 加密的key/value数据
2. ConfigMap              // 不加密的配置数据
3. Downward API           // 可以引用pod中的元信息(比如label、nodename、hostIp、namespace、podip、uid、annotations等等)
4. ServiceAccountToken    // 本质上是一种特殊的Secret



PodPreset用于给开发人员写的pod添加一些预设的字段

```
apiVersion: settings.k8s.io/v1alpha1
kind: PodPreset
metadata:
  name: allow-database
spec:
  selector:
    matchLabels:
      role: frontend
  env:
    - name: DB_PORT
      value: "6379"
  volumeMounts:
    - mountPath: /cache
      name: cache-volume
  volumes:
    - name: cache-volume
      emptyDir: {}
```


## 控制器
ReplicationController: 已经废弃
ReplicaSet: 自动扩缩容
Deployment: 控制ReplicaSet来间接的控制pods
DaemonSet:
Job:
Crontab:
StatefulSet

## ReplicaSet
spec:
  replicas: 副本数量
  selector: 选择pod
    matchLabels:
      LABEL
  template: 容器模版
    metadata:
      name:
      labels: 和上面的标签选择器要一致
        LABEL
    spec:
      containers:
      -name:
       image:
       ports:
       -name:
        containerPort

## Deployment
1. 更新的粒度(一批更新多少)
2. 更新的过程是否允许超过期望的Pod数量
3. 灰度过程中占停
4. 蓝色部署
5. 回滚

example:
apiVersion: apps/v1
kind: Deployment
metadata:
  name: demo
  namespace: default
  labels:
  annotations:
spec:
  replicas: 2
  rollingUpdatesStrategy:
  strategy: 更新策略
    rollingUpdate:
      maxSurge : 最多可超过的容器数量/比例
      maxUnavailable: 最多不可用的容器数量
    type: Recreate|RollingUpdate
  revisionHistoryLimit: 保存的历史版本数量
  selector:
    matchLabels:
      LABEL
  template: 容器模版
    metadata:
      name:
        labels:
          LABEL
    spec:
      -name:
       image:
       ports:
       -name:
        containerPort

> CDR(Custom Defined Resources) 1.8+
> Operator
> Helm 类似于Yum
> kubectl apply 声明式，同一份yaml可以apply多次，每次都会记录在annotations中
> kubectl get pods -w 动态监控
> kubectl set image TYPE NAME=IMAGE-NAME
> kubectl rollout pause TYPE NAME 停止
> kubectl rollout resume TYPE NAME 恢复
> kubectl rollout history TYPE NAME 查看更新历史
> kubectl rollout undo 回滚
> kubectl patch TYPE -p '{}'
> kubectl rollout history TYPE NAME  查看回滚的revision
> kubectl rollout undo TYPE NAME --to-revision=REVISION_NAME

## DaemonSet

DaemonSet在实现的时候会给每一个Pod加上一个nodeAddinity从而保证这个Pod只会在指定的节点上启动

```yaml
apiVersion: apps/v1
kind: DaemonSet
metadata:
  name: fluentd-elasticsearch
  namespace: kube-system
  labels:
    k8s-app: fluentd-logging
spec:
  selector:
    matchLabels:
      name: fluentd-elasticsearch
  template:
    metadata:
      labels:
        name: fluentd-elasticsearch
    spec:
      # 需要有一个容忍的污点，可以在master上安装，默认master是不会进行pod调度的。
      # 当一个节点没有安装网络插件会被设置node.kubernetes.io/network-unavailable污点
      # 所以在安装网络agent插件的时候，需要容忍这个污点
      tolerations:
      - key: node-role.kubernetes.io/master
        effect: NoSchedule
      containers:
      - name: fluentd-elasticsearch
        image: k8s.gcr.io/fluentd-elasticsearch:1.20
        resources:
          limits:
            memory: 200Mi
          requests:
            cpu: 100m
            memory: 200Mi
        volumeMounts:
        - name: varlog
          mountPath: /var/log
        - name: varlibdockercontainers
          mountPath: /var/lib/docker/containers
          readOnly: true

      terminationGracePeriodSeconds: 30
      volumes:
      - name: varlog
        hostPath:
          path: /var/log
      - name: varlibdockercontainers
        hostPath:
          path: /var/lib/docker/containers
```



## Service
1. userspace: 先到service ip，然后转到node上的kube-proxy，由kube-proxy向后转发
2. iptables: 直接iptables转发，不需要kube-proxy
3. ipvs:

apiVersion: v1
kind: Service
metadata:
  name: redis
  namespace: default
spec:
  ports:
    name:
    nodePort: 在集群外被访问的port，默认会自动分配的
    port: service ip上的port
    protocol:
    targetPort: 容器中的port
  selector:
  externalName:  ExternName类型: CNAME记录，可以被互联网真正解析的，用于访问外部服务，将外部服务的名称映射到内部服务
  clusterIP: "None"表示headless的服务，服务名直接解析为后端对应的pod
  sessionAddinity: None|ClientIP 根据源IP来负载均衡，保持会话粘性
  type: ExternName(CNAME，指向真正的FQDN), ClusterIP, NodePort, LoadBlancer

Servive -> EndPoint资源对象 -> Pod

资源记录: SVC_NAME.NS_NAME.DOMAIN.LTD  (svc.cluster.local.)

## Ingress

ingress controller和Deployment不相同，不存在controller manager，是独立存在的POD资源

`/etc/sysconfig/kubelet` 添加
KUBE_PROXY_MODE=ipvs 来设置kube-proxy的工作模式为IPVS

ingress pod可以直接复用宿主机地址，避免nodeIp:nodePort到clusterIP:servicePort的转换

```
kubectl create namespace xxx
kubectl delete namespace xxx
```

example:
apiVersion: extensions/v1beta1
kind: Ingress
metadata:
  name: xxx
  namespace: default
  annotations: kubernetes.io/ingress.class: "nginx"
spec:
  backend: 默认
    serviceName:
    servivcePort:
  tls:
    hosts:
    - xxxx
    - xxxx
    secretName:
  rules:
   host:
   http
     paths:
     - path
       backend:
         serviceName:
         servicePort:


自签证书:
openssl genrsa -out tls.key 2048
openssl req -new -x509 -key tls.key -out tls.crt -subj /C=CN/ST=Beijijng/L=xxxxxx

创建secret对象:
kubectl create secret tls NAME --cert=PATH --key=PATH

## 存储卷

1. emptyDir
2. hostPath
3. SAN(iSCSI)、NAS、分布式存储(glusterfs、ceph-rdb、cephfs)、云存储(EBS、OSS、Azure Disk)

example: 基于宿主机来共享存储
apiVersion: v1
kind: Pod
metadata:
  name: NAME
  namespace: NAMESPACE
spec:
  containers:
  - name
    image:
    volumeMounts:
    - name:
      mountPath:
  volumes:
  -name:
   hostPath:
     path:
     type: DirectoryOrCreate

PVC: 封装存储的细节，也是k8s中的一个资源

spec:
  accessModes: 是PV的子集
  resources:
    requests:
      storage:
  selector:
  strageClassName:
  volumeMode:
  volumeName:

PV:
apiVersion: v1
kind: PersistentVolume
metadata:
spec: 和pods.spec.volumes相同
  reclamPolicy: retain、recyle、delete
  accessModes:
  capacity:
    storage: 20Gi

PVC根据requests来选择PV，PV的大小是事先分配好的。
PVC如何进行动态的创建PV呢?

Dynamic Provisioning

1. PVC找到StorageClass
2. StorageClass存储插件创建PV


配置容器化应用的方式:

1. 自定义命令行参数
2. 把配置文件直接放进镜像
3. 环境变量
  spec.containers.env
4. 存储卷

## ConfigMap

1. 环境变量
2. 直接当作存储卷

apiVersion:
binaryData: <map[string]string>
data: <map[string]string>
kind:
metadata:

kubectl create configmap NAME --from-file|--from-literal=KEY=VALUE
kubectl get cm
kubectl describe cm NAME

pods.spec.containers.env.valueFrom.configMapKeyRef.{name, key, optional} 只在启动的时候有效
pods.spec.containers.volumes.item 指定哪些key被挂载


## secret

1. docker registers
2. general
3. tls

## stateful

1. 稳定且唯一的网络标识符
2. 稳定且持久的存储
3. 有序、平滑地部署和扩展
4. 有序、平滑地删除和终止
5. 有序的滚动更新

三个组件: headless service、StatefulSet、volumeClaimTemplate
名称解析: pod_name.service_name.ns_name.svc.cluster.local


## controllerrevision

专门记录某种controller对象的版本，像statefulset、daemonset的版本都是通过这个对象来管理的
这个对象的Data字段保存了该版本对应的完整的DaemonSet的API对象，并且在Annotation字段保存了创建这个对象所使用的kubectl 命令


## Job && CronJob

```yaml
apiVersion: batch/v1
kind: Job
metadata:
  name: pi
spec:
  # 要处理的任务数目是8
  completions: 8
  # 同时执行的任务数是2
  parallelism: 2
  template:
    spec:
      containers:
      - name: pi
        image: resouer/ubuntu-bc
        command: ["sh", "-c", "echo 'scale=10000; 4*a(1)' | bc -l"]
      restartPolicy: Never
  # 最大重试次数4次(当任务执行失败的时候进行重试)
  backoffLimit: 4
```

```yaml
apiVersion: batch/v1beta1
kind: CronJob
metadata:
  name: hello
spec:
  # 1. concurrencyPolicy=Allow，这也是默认情况，这意味着这些 Job 可以同时存在;
  # 2. concurrencyPolicy=Forbid，这意味着不会创建新的 Pod，该创建周期被跳过;
  # 3. concurrencyPolicy=Replace，这意味着新产生的 Job 会替换旧的、没有执行完的 Job。
  concurrencyPolicy: Forbid
  # 意味着在过去 200 s 里，如果 miss 的数目达到了 100 次， 那么这个 Job 就不会被创建执行了。
  startingDeadlineSeconds: 200
  schedule: "*/1 * * * *"
  jobTemplate:
    spec:
      template:
        spec:
          containers:
          - name: hello
            image: busybox
            args:
            - /bin/sh
            - -c
            - date; echo Hello from the Kubernetes cluster
          restartPolicy: OnFailure
```




## Admission Controllers

API对象被提交给ApiServer后做的一系列操作被称为Admission Controllers，需要给ApiServer编写代码来实现功能，默认ApiServer提供了
许多Admission Controllers插件，但是仍然没办法满足一些需求，而且每次新增插件都需要重启。因此有了Dynamic Admission Controllers，可以热插拔。

Initializer插件就是其中一种，在v1alpha1中存在，默认是disabled的。

## 声明式API

一个API对象在Etcd中的完整资源路径: Group + Version + Resource组成

默认核心组: /api，查找的时候默认省略，比如Pod、Node等，非核心的API对象则在/apis这个层级查找

一个对象提交到ApiServer的完整过程:
1. 发起POST请求，发送yaml到ApiServer中
2. 过滤请求(授权、超时、审计)
3. Mux、Routes
4. 转换成Super Version(API资源类型的所有版本的全字段全集)
5. Admission和Validation



## CRD

一个CRD资源对象，资源类型是Network，组是samplecrd.k8s.io，版本是v1

```yaml
apiVersion: samplecrd.k8s.io/v1
kind: Network
metadata:
  name: example-network
spec:
  cidr: "192.168.0.0/16"
  gateway: "192.168.0.1"
```

资源描述

```yaml
apiVersion: apiextensions.k8s.io/v1beta1
kind: CustomResourceDefinition
metadata:
  # 资源名(复数形式).组名
  name: networks.samplecrd.k8s.io
spec:
  # 组名
  group: samplecrd.k8s.io
  # 版本
  version: v1
  names:
    # 资源名
    kind: Network
    # 复数
    plural: networks
  # 作用域
  scope: Namespaced
```

操作CRD资源:

1. 目录结构

```
controller.go
  crd
    network.yaml  # CRD文件
  example
    example-network # Network资源文件
  main.go
  pkg
    apis
      samplecrd
        register.go
        v1
          doc.go
          register.go
          types.go
```

2. `pkg/apis/samplecrd/register.go`

一些全局变量的定义，定义GroupName、Version等

```go
package samplecrd

const (
        GroupName = "samplecrd.k8s.io"
        Version   = "v1"
)
```

## RBAC

1. Role: 角色，它其实就是一组规则，定义一组对Kubernetes API对象的操作权限
2. Subject: 被作用者， 既可以是人、也可以是机器，也可以是你在Kubernetes里定义的 "用户"
3. RoleBinding: 定义了作用者和角色的绑定关系

ClusterRole和ClusterRoleBinding作用于整个集群，在metadata中不需要指定namespace

```yaml
kind: Role
apiVersion: rbac.authorization.k8s.io/v1
metadata:
  namespace: mynamespace
  name: example-role
rules: # 限制哪个api、哪个资源类型、允许的操作。
- apiGroups: [""]
  resources: ["pods"]
  verbs: ["get", "watch", "list"]
```

将role绑定到k8s中的User

```yaml
kind: RoleBinding
apiVersion: rbac.authorization.k8s.io/v1
metadata:
  name: example-rolebinding
  namespace: mynamespace
subjects:
  # 授权系统中的逻辑概念，它需要通过外部认证服务来进行授权
  # 还可以使用k8s管理的内置用户，也就是ServiceAccount
- kind: User
  name: example-user
  # ? 为什么要加这个apiGroup
  #  APIGroup holds the API group of the referenced subject. Defaults to "" for
  #  ServiceAccount subjects. Defaults to "rbac.authorization.k8s.io" for User
  #  and Group subjects.
  apiGroup: rbac.authorization.k8s.io
# 引用role
roleRef:
  kind: Role
  name: example-role
  apiGroup: rbac.authorization.k8s.io
```

对于ServiceCount来说，k8s会给每一个ServiceCount自动创建并分配一个Secret对象。这个对象就是用来和APIServer进行交互的授权文件，一般称为token
用户的POD可以指定使用这个ServiceCount，否则k8s会把一个叫default的service count绑定到pod中。

```yaml
apiVersion: v1
kind: Pod
metadata:
  namespace: mynamespace
  name: sa-token-test
spec:
  containers:
  - name: nginx
    image: nginx:1.7.9
  serviceAccountName: example-sa
```

使用ServiceCount Group

```yaml
subjects:
 - kind: Group
 # system:serviceaccounts:<Namespace 名字>
 name: system:serviceaccounts:mynamespace
 apiGroup: rbac.authorization.k8s.io
```

## 日志收集

1. Node上部署logging agent，将日志文件转发到后端存储里面，要求容器中的应用输出日志到stdout和stderr
2. SideCar容器将应用容器中的日志文件输出到stdout和stderr，然后再有logging agent采集
3. SideCar容器部署logging直接将应用容器产生的日志文件采集到远端存储