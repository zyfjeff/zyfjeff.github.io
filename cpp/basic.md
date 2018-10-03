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


参考文章:
1. https://www.fluentcpp.com/2016/12/05/named-constructors/
2. https://foonathan.net/blog/2016/10/19/strong-typedefs.html

## Curiously Recurring Template Pattern (CRTP)
递归的奇异模版，有两个特点:
1. 从模版类继承
2. 使用派生类自己作为基类的模版参数

```cpp
template <typename T>
class Base {};

class Dervived : public Base<Dervived> {};
```

这样做的目的就是为了能在基类中使用派生类，这样基类就可以通过`static_cast`来使用派生类。

```cpp
template <typename T>
class Base {
 public:
  void doSomething() {
    T& derived = static_cast<T&>(*this);
  }
};
```
但是这存在一个问题，如果基类模版参数弄错了怎么办?，例如下面这个例子:

```cpp
class Derived1 : public Base<Derived1> {};
class Derived2 : public Base<Derived1> {};// bug in this line of code
```

`Marek Kurdej`提出了一个解决方案，在基类中将对应的派生类设置为friend;

```cpp
template <typename T>
class Base {
public:
private:
    Base(){};
    friend T;
};
```
这样的化，，如果模版类的参数是错误的就会导致基类构造失败，因为基类的构造函数是私有的，只有其友元函数可以派生自基类

CRTP的用处很大，这里列举两个场景:

1. 给派生类添加功能

```cpp
template <typename T>
struct AddOperator {
  T operator+ (const T& t) const {
    return T(t.value() + value());
  }

  int value() const { return static_cast<const T*>(this)->value(); }
};

class test : public AddOperator<test> {
 public:
  test(int value) : value_(value) {}
  int value() const { return value_; }
 private:
  int value_;
};

int main() {
  test t1(11), t2(20);
  std::cout << (t1 + t2).value() << std::endl;
  return 0;
}
```

2. 静态多态

```cpp
template <typename T>
class Amount {
 public:
  double getValue() const {
    return static_cast<T const&>(*this).getValue();
  }
};

class Constant42 : public Amount<Constant42> {
 public:
  double getValue() const { return 42; }
};

class Variable : public Amount<Variable> {
 public:
  explicit Variable(int value) : value_(value) {}
  double getValue() const { return value_; }
 private:
  int value_;
};

template<typename T>
void print(Amount<T> const& amount) {
  std::cout << amount.getValue() << "\n";
}

Constant42 c42;
print(c42);
Variable v(43);
print(v);
```

上面的代码在基类中调用派生类的特定方法(`getValue`)，所有继承自这个基类的派生类都包含了这个方法。
但是这些派生类的父类不相同，不能像正常的多台方式(基类指针指向派生类)来调用，但是它们的基类都是`Amount<T>`，
这样就可以通过模版的方式来调用这些派生类。上面的print函数模版就是一个典型的例子。

仔细观察Amount模版类可以发现有一个共同点就是会通过static_cast来将基类转化为派生类，这个可以抽离成一个公共模块。

```cpp
template <typename T>
struct crtp
{
    T& underlying() { return static_cast<T&>(*this); }
    T const& underlying() const { return static_cast<T const&>(*this); }
};

template<typename T>
class Amount : public crtp<T> {
  double getValue() const {
    return this->underlaying().getValue();
  }
}
```

但是上面会遇到菱形继承的问题

```cpp
template <typename T>
struct Scale : crtp<T> {
 void scale(double multiplicator) {
   this->underlying().setValue(this->underlying().getValue() * multiplicator);
 }
};

template <typename T>
struct Square : crtp<T> {
 void square() {
   this->underlying().setValue(this->underlying().getValue() * this->underlying().getValue());
 }
};

class Sensitivity : public Scale<Sensitivity>, public Square<Sensitivity> {
 public:
  double getValue() const { return value_; }
  void setValue(double value) { value_ = value; }

private:
  double value_;
};
```

上面的代码中最终的基类都是`crtp<Sensitivity>`,为了解决这个问题可以给crtp添加一个额外的模版参数来避免

```cpp
template <typename T, template<typename> class crtpType>
struct crtp {
 public:
  T& underlying() { return static_cast<T&>(*this); }
  const T& underlying() const { return static_cast<const T&>(*this); }
 private:
  crtp() {}
  friend crtpType<T>;
};
```
