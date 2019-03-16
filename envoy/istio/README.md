## 前置条件

1. Named ports: 定义service的时候，其中port字段需要使用`<protocol>[-<suffix>]`的形式，其中protocol需要是http、http2、grpc、mongo、redis等目的是为了使用istio的路由特性，否则会被认为是TCP流量。
2. Service association: 一个POD如果关联多个service的时候，这个service不能是相同的port但却是不同的协议
3. Deployments with app and version labels: pods部署的时候使用Kubernetes Deployment需要显示的打上app和version的标签。这些标签用于分布式tracing和metric telemetry采集。

