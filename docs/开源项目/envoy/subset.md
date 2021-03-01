# Subset Load Balancer分析

## Cluster subset

本质上是将一个集群拆分成多个子集群，Envoy中称为subset，而subset的划分是根据每一个endpoint中携带的metadata来组织subset集合。

```yaml
{
  "fallback_policy": "...",
  "default_subset": "{...}",
  "subset_selectors": [],
  "locality_weight_aware": "...",
  "scale_locality_weight": "...",
  "panic_mode_any": "..."
}
```

* fallback_policy:

    1. NO_ENDPOINT 如果没有匹配的subset就失败
    2. ANY_ENDPOINT 如果没有匹配的subset就从整个集群中按照负载均衡的策略来选择
    3. DEFAULT_SUBSET 匹配指定的默认subset，如果机器都没有指定subset那么就使用ANY_ENDPOINT

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

Reference: https://github.com/envoyproxy/envoy/blob/master/source/docs/subset_load_balancer.md