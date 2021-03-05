# Subset Load Balancer分析

Subset本质上是将一个集群拆分成多个子集，在Envoy中这个子集称为subset，而subset的划分是根据每一个endpoint中携带的metadata来组织subset的。有了这些subset后，在路由时会根据
请求中携带的metadata来选择路由到哪个subset集合。最后再根据集群指定的负载均衡算法从subset中选择一台机器。

## Subset配置分析

```yaml
{
  "fallback_policy": "...",
  "default_subset": "{...}",
  "subset_selectors": [],
  "locality_weight_aware": "...",
  "scale_locality_weight": "...",
  "panic_mode_any": "...",
  "list_as_any": "..."
}
```

* fallback_policy:

    1. NO_FALLBACK 如果没有匹配的subset就失败，返回no healthy upstream类似的错误。
    2. ANY_ENDPOINT 如果没有匹配的subset就从整个集群中按照负载均衡的策略来选择。
    3. DEFAULT_SUBSET 如果没有匹配的subset，就使用默认配置的subset，如果配置的默认subset也不存在就等同于NO_FALLBACK

* default_subset

指定一个set集合，用来在fallback的时候，进行选择

* locality_weight_aware


* subset_selectors

用来表明subset如何生成，下面这个例子就是表明带有a和b的metadata的机器会组成一个subset。如果一个机器带有a、b、x三个metadata，
那么这台机器就会属于两个subset。

```json
{
  "subset_selectors": [
    { "keys": [ "a", "b" ] },
    { "keys": [ "x" ] }
  ]
}
```
* locality_weight_aware

* scale_locality_weight

* panic_mode_any


默认子集不为空，但是通过fallback策略使用默认子集的时候，仍然没有选择到主机，那么通过这个选项会使得从所有机器中选择一台机器来进行路由


```C++
  if (cluster->lbSubsetInfo().isEnabled()) {
    lb_ = std::make_unique<SubsetLoadBalancer>(
        cluster->lbType(), priority_set_, parent_.local_priority_set_, cluster->stats(),
        cluster->statsScope(), parent.parent_.runtime_, parent.parent_.random_,
        cluster->lbSubsetInfo(), cluster->lbRingHashConfig(), cluster->lbMaglevConfig(),
        cluster->lbLeastRequestConfig(), cluster->lbConfig());
  }
  .....
```


## 例子


* Example

Endpoint | stage | version | type   | xlarge
---------|-------|---------|--------|-------
e1       | prod  | 1.0     | std    | true
e2       | prod  | 1.0     | std    |
e3       | prod  | 1.1     | std    |
e4       | prod  | 1.1     | std    |
e5       | prod  | 1.0     | bigmem |
e6       | prod  | 1.1     | bigmem |
e7       | dev   | 1.2-pre | std    |

Note: Only e1 has the "xlarge" metadata key.

Given this CDS `envoy::api::v2::Cluster`:

``` json
{
  "name": "c1",
  "lb_policy": "ROUND_ROBIN",
  "lb_subset_config": {
    "fallback_policy": "DEFAULT_SUBSET",
    "default_subset": {
      "stage": "prod",
      "version": "1.0",
      "type": "std"
    },
    "subset_selectors": [
      { "keys": [ "stage", "type" ] },
      { "keys": [ "stage", "version" ] },
      { "keys": [ "version" ] },
      { "keys": [ "xlarge", "version" ] },
    ]
  }
}
```

下面这些subset将会生成:

* stage=prod, type=std (e1, e2, e3, e4)
* stage=prod, type=bigmem (e5, e6)
* stage=dev, type=std (e7)
* stage=prod, version=1.0 (e1, e2, e5)
* stage=prod, version=1.1 (e3, e4, e6)
* stage=dev, version=1.2-pre (e7)
* version=1.0 (e1, e2, e5)
* version=1.1 (e3, e4, e6)
* version=1.2-pre (e7)
* version=1.0, xlarge=true (e1)

默认的subset
stage=prod, type=std, version=1.0 (e1, e2)


Example:

```yaml
    filter_chains:
    - filters:
      - name: envoy.http_connection_manager
        config:
          codec_type: auto
          stat_prefix: ingress_http
          route_config:
            name: local_route
            virtual_hosts:
            - name: backend
              domains:
              - "*"
              routes:
              - match:
                  prefix: "/"
                route:
                  cluster: service1
                  metadata_match:
                    filter_metadata:
                      envoy.lb:
                        env: taobao
          http_filters:
          - name: envoy.router
            config: {}
  clusters:
  - name: service1
    connect_timeout: 0.25s
    type: strict_dns
    lb_policy: round_robin
    lb_subset_config:
      fallback_policy: DEFAULT_SUBSET
      default_subset:
        env: "taobao"
      subset_selectors:
      - keys:
        - env
    load_assignment:
      cluster_name: service1
      endpoints:
        lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: 8081
          metadata:
            filter_metadata:
              envoy.lb:
                env: hema

        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: 8082
          metadata:
            filter_metadata:
              envoy.lb:
                env: taobao
```