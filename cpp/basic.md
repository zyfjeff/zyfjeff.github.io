# 最佳实践

## 使用duration_cast使得时间转化的代码可读性更高

```cpp
std::chrono::microseconds us = std::chrono::duration_cast<std::chrono::microseconds>(d);
timeval tv;
tv.tv_sec = us.count() / 1000000; # 这种操作易读性不高
```

换成下面这种形式

```cpp
auto secs = std::chrono::duration_cast<std::chrono::seconds>(d);
auto usecs = std::chrono::duration_cast<std::chrono::microseconds>(d - secs);
tv.tv_secs = secs.count();
tv.tv_usecs = usecs.count();
```

## 使用Gdb来dump内存信息

```
sudo gdb
> attach 进程PID
> i proc mappings  # 查看进程的地址空间信息 等同于 /proc/PID/maps
> i files  # 可以看到更为详细的内存信息 等同于 /proc/PID/smaps
> dump memory 要存放的位置 SRC DST # 把SRC到DST 地址范围内的内存信息dump出来

对dump出来的内容实用strings即可查看到该段内存中存放的字符串信息了
```

## 引用折叠和cv限制符

引用折叠规则:
1. 所有右值引用折叠到右值引用上仍然是一个右值引用
2. 所有的其他引用类型之间的折叠都将变成左值引用

但是当发生引用折叠的时候，cv限制符会被去掉，例如下面这个例子:

```cpp
template<typename T>
void refFold(const T& data) {}

int c = 0;
refFold<int&>(c);
refFold<int&>(0); # compile error
```

## 强类型

