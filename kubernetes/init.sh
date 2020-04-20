#!/bin/bash

# 1. epel source
wget -O /etc/yum.repos.d/epel.repo http://mirrors.aliyun.com/repo/epel-7.repo
# 2. chrony service
yum install chrony -y
systemctl start chronyd.service
systemctl enable chronyd.service

# 3. stop firewald and iptables
systemctl stop firewalld.service iptables.service
systemctl disable firewalld.service iptables.service

# 4. stop selinux
setenforce 0
# sed -i 's@^\(SELINUX=\).*@\1disabled@' /etc/sysconfig/selinux

# 5. disbale swap device
swapoff -a

# 6. enable ipvs modules

# 7. install docker
wget "https://download.docker.com/linux/centos/docker-ce.repo" -O /etc/yum.repos.d/docker-ce.repo
yum install container-selinux -b current -y
yum install docker-ce -y

> http://ftp.riken.jp/Linux/cern/centos/7/extras/x86_64/Packages/ 可能要更新selinux的版本

# service section
# vim /usr/lib/systemd/system/docker.service
# ExecStartPost=/usr/sbin/iptables -P FORWARD ACCEPT
# Environment="HTTP_PROXY=http://30.57.177.111:3128"
# Environment="HTTPS_PROXY=http://30.57.177.111:3128"

mkdir -p /etc/docker/
tee /etc/docker/daemon.json <<-'EOF'
{
  "registry-mirrors": ["https://fajbo8jx.mirror.aliyuncs.com"]
}
EOF
systemctl daemon-reload
systemctl start docker.service
systemctl enable docker.service

# 8. install kubeadm
sudo cat <<EOF > /etc/yum.repos.d/kubernetes.repo
[kubernetes]
name=Kubernetes
baseurl=https://mirrors.aliyun.com/kubernetes/yum/repos/kubernetes-el7-x86_64/
enabled=1
gpgcheck=1
repo_gpgcheck=1
gpgkey=https://mirrors.aliyun.com/kubernetes/yum/doc/yum-key.gpg https://mirrors.aliyun.com/kubernetes/yum/doc/rpm-package-key.gpg
EOF

yum install kubelet kubeadm kubectl -y

# 9. configure kubelet
# vim /etc/sysconfig/kubelet
# KUBELET_EXTRA_ARGS="--fail-swap-on=false"
systemctl enable kubelet.service
echo 1 > /proc/sys/net/bridge/bridge-nf-call-iptables
kubeadm init --kubernetes-version=v1.13.2 --pod-network-cidr=10.244.0.0/16 --service-cidr=10.96.0.0/12 --apiserver-advertise-address=0.0.0.0 --ignore-preflight-errors=Swap


# 节点加入
mkdir -p $HOME/.kube
sudo cp -i /etc/kubernetes/admin.conf $HOME/.kube/config
sudo chown $(id -u):$(id -g) $HOME/.kube/config
kubeadm join 11.239.162.175:6443 --token nnpkt1.jd5rkkx7kpweqqhp --discovery-token-ca-cert-hash sha256:88ab00297364ab322dbbef8c235aee61d5d6a1d2c43fa6c53ccfa72bd9e14fa0

# remove node
kubectl drain NODE_ID --delete-local-data --force --ignore-daemonsets
kubectl delete node NODE_ID

# node reset
kubeadm reset


sudo kubeadm join 11.164.60.62:6443 --token wzalxi.q55omjmlgwisxsa0 --discovery-token-ca-cert-hash sha256:130420acbb5b2af96c35c99d0eb98f1e6aa8224ea53cc17459827433fb7398b2 -v=5