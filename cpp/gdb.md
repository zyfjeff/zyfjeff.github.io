## 安装pretty script

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

## 常用Command

* info vbtl VAR 查看虚表
