
## Setp1

```
sudo bash istio-iptables.sh
```

默认会做几下几件事:


1. nat表中 `-A OUTPUT -p tcp -j ISTIO_OUTPUT` 输出的流量走到`ISTIO_OUTPUT`链
2. `-A ISTIO_IN_REDIRECT -p tcp -j REDIRECT --to-ports 15001` 进入到重定向链的流量端口都重定向到15001，也就是envoy的端口