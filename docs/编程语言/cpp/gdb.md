
## Install pretty script

1. 下载对应gcc版本的pretty script

  *  `/opt/rh/devtoolset-8/root/usr/share/gdb/python/libstdcxx/v6/`
  * `/usr/share/gcc-4.8.5/python/libstdcxx/v6/`

> python module路径是从libstdcxx/v6/开始的，因此从这里开始要构建好完整的目录

2. 创建.gdbinit文件

```python
python
import sys
sys.path.insert(0, '/root/python')
from libstdcxx.v6.printers import register_libstdcxx_printers
register_libstdcxx_printers (None)
end
```

## Command

* info vbtl VAR 查看虚表

## Tips

1. set print object on
2. ptype obj/class/struct
3. set print pretty on
4. set print vtbl on
5. handle SIGUSR1 nostop/ignore


## 使用Gdb来dump内存信息

```
sudo gdb
> attach 进程PID
> i proc mappings  # 查看进程的地址空间信息 等同于 /proc/PID/maps
> i files  # 可以看到更为详细的内存信息 等同于 /proc/PID/smaps
> dump memory 要存放的位置 SRC DST # 把SRC到DST 地址范围内的内存信息dump出来

对dump出来的内容实用strings即可查看到该段内存中存放的字符串信息了
```