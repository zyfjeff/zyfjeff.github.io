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


## 资源对象
1. workload(工作负载型资源): Pod、ReplicaSet、Deplovment、StatefulSet、DaemonSet、Job、Cronjob
2. 服务发现和均衡: Service、Ingress
3. 配置与存储: Volume、CSI、ConfigMap、Secret、DownwardAPI
4. 集群级资源: Namespace、Node、Role、ClusterRole、RoleBinding、ClusterRoleBinding
5. 元数据型资源: HPA、PodTemplate、LimitRange

## 资源定义
ApiServer仅接收JSON格式的资源定义:(yaml更易读，可以无损转化为JSON)
大部分资源的配置清单都由五个部分组成:
1. apiVersion: group/version (kubectl api-versions，省略掉组的话就是core类型的核心组)
2. kind: 资源类型
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
    ports: <[]Object> 容器暴露的端口，只是信息暴露，通过yaml知道这个应用暴露了哪些端口
      containerPort:
      hostIP:
      hostPort:
      name:
      protocol: 默认TCP
    args: <[]string> 向entrypoint传递参数，代替镜像中的CMD参数
    command: <[]string> 相当于entrypoint，是不会运行在shell中的，如果不提供就运行镜像中的entrypoint
  nodeSelector: <map[string]string> 选择要运行在哪类node上
  nodeName: <string> 指定要运行的node机器上
  hostPID:
  hostIPC:
  hostNetwork:
  restartPolicy: Always、OnFailure、Never、Default to Always
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

状态:
  1. Pending
  2. Running
  3. Failed
  4. Succeeded
  5. Unknown

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

spec:
  type:
  rollingUpdate:
    maxUnavailable:

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
1. 有状态
2. 不同的角色、顺序依赖等



1. 稳定且唯一的网络标识符
2. 稳定且持久的存储
3. 有序、平滑地部署和扩展
4. 有序、平滑地删除和终止
5. 有序的滚动更新


三个组件: headless service、StatefulSet、volumeClaimTemplate
名称解析: pod_name.service_name.ns_name.svc.cluster.local



