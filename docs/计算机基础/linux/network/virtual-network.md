## Bridge



## vxlan

1. 创建点对点的类型的vxlan，

```shell
$ ip link add vxlan0 type vxlan \
    id 42 \
    remote 192.168.8.101 \
    local 192.168.8.100 \
    dev enp0s8
```

2. 给vxlan设置IP，并开启vxlan接口

```shell
$ ip addr add 10.20.1.2/24 dev vxlan0
$ ip link set vxlan0 up
```

执行完成后会在路由表中添加如下路由条目:

```shell
10.20.1.0/24 dev vxlan0  proto kernel  scope link  src 10.20.1.2
```

https://developers.redhat.com/blog/2018/10/22/introduction-to-linux-interfaces-for-virtual-networking/