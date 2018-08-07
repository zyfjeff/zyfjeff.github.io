## IP_TRANSPARENT选项的作用?

`IP_TRANSPARENT`的目的是用来实现所谓的透明代理，和其对应的就是传统的正向代理，需要用户感知代理的存在，用户访问的其实是代理服务器，代理服务器间接的请求真正的服务，然后将结果传递给用户，而透明代理是不会让用户感知的，要做到这点其实就是需要透明的拦截掉用户的流量，将流量导向到代理服务器实现代理。比如用户访问8.8.8.8的80端口，那么通过一些手段(比如默认网关)将请求转发到代理服务器上，那么代理服务器需要accept这个请求，但是这个请求访问的地址其实是`8.8.8.8`，正常情况下是无法accept这个请求的。可以通过下面这个例子来验证：

先实现一个模拟的代理服务器，只负责接受链接。
```cpp
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>


int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  int value = 1;
  int ret;
  ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
  perror("setsockopt:");
  
  struct sockaddr_in name;
  name.sin_family = AF_INET;
  name.sin_port = htons(9999);
  name.sin_addr.s_addr = htonl(INADDR_ANY);
  
  ret = bind(fd, (struct sockaddr*)&name, sizeof(name));
  perror("bind:");
  
  struct sockaddr_in remote_addr;
  socklen_t addr_len = sizeof(sockaddr_in);
  
  ret = listen(fd, 128);
  perror("listen:");

  ret = accept(fd, (struct sockaddr*)&remote_addr, &addr_len);
  perror("accept");
  char ipaddr[20] = {0};
  printf("%s\n", inet_ntop(AF_INET, (void*)&remote_addr.sin_addr, ipaddr, 16));
}


```

添加默认网关，所有的流量都导向本地的`127.0.0.1`

```bash
sudo route add default gw 127.0.0.1
```

模拟用户访问一个远程地址比如`8.8.8.8`的`9999`端口

```bash
nc 8.8.8.8 9999
```

虽然用户的请求被拦截了，并发向了本机的代理服务器上，但是用户的这个请求目的地址是`8.8.8.8`，并不是代理服务器的地址，默认情况下代理服务器无法接受这个请求的，这个是因为当一个请求被内核接受的时候会先查询下`local`路由表查看目的地址是否在路由表中，如果在才会接受链接，否则是不会接受的。所以为了让代理服务器接受请求可以在代理服务器的机器上将`8.8.8.8`添加到`local`路由表中。

将`8.8.8.8`添加到`local`路由表

```bash
ip route add to local 8.8.8.8 dev lo
```

再次连接`8.8.8.8`那么这个请求就会顺利被代理程序所接受。另外一个办法就是本文所描述的`IP_TRANSPARENT`，通过给`socket`设置这个选项后代理程序就可以接受任何非`local`路由表中出现的地址，就可以实现透明代理了。

给socket设置`IP_TRANSPARENT`
```bash
int value = 1;
int ret;
ret = setsockopt(fd, SOL_IP, IP_TRANSPARENT, &value, sizeof(value));
```

#### Reference

* [Linux路由分析 - Just For Coding](http://www.just4coding.com/blog/2016/12/10/linux-route/)
* [Transparent proxy support](https://www.kernel.org/doc/Documentation/networking/tproxy.txt)
* [图解正向代理、反向代理、透明代理-丁胖胖的BLOG-51CTO博客](http://blog.51cto.com/z00w00/1031287)
