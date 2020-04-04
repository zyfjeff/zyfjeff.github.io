## SOLID

* Single Responsibility Principle
* Open-Closed Principle
* Liskov Substituion Principle
* Interface Segregation Principle
* Dependency Inversion/Injection

## Factory

* Builder

```cpp
HtmlBuilder builder{"ul"};
builder.add_child("li", "hello");
builder.add_child("li", "world");
cout << builder.str() << endl;
```

* Fluent Builder

```cpp
 HtmlElement htmlElement1 =
     HtmlElement::build("ul").add_child("li", "hello").add_child("li", "world");
 cout << htmlElement1.str() << endl;
```


* Groovy Style Builder

```cpp
 std::cout << P{IMG{"http://pokemon.com/pikachu.png"},
               P{IMG{"http://pokemon.com/pikachu.jpg"}}}
          << std::endl;
```

* Builder Facets

```cpp
Person p = Person::create()
               .lives()
               .at("123 London Road")
               .with_postcode("SW1 1GB")
               .in("London")
               .works()
               .at("PragmaSoft")
               .as_a("Consultant")
               .earning(10e6);
cout << p << endl;
```

多种Factory的实现:

1. 类的静态方法
2. 独立的Factory类
3. 内部类
4. 抽象的Factory

```cpp
DrinkFactory df;
df.make_drink("coffee");
```

## Prototype
A partially or fully initialized object that you copy/clone and make use of.


## Bridge

* PIMPL = Pointer to an Implementation


## flyweight(享元模式)


## Visitor

## Strategy

## State

## Observer


## Callback Pattern

1. 直接定义好一个callback，两个类之间的依赖通过callback来进行

```cpp
class PersonA {
 using UpdateNameCallback =  std::function<bool(const std::string&)>;
  void set_update_name_callback(UpdateNameCallback callback) {
    UpdateNameCallback = &callback;
  }
 UpdateNameCallback* update_name_callback_;
};
```

其他的模块可以通过`set_update_name_callback`来设置callback进行回调，进而来进行解耦。



## Strategized Locking

基于Strategy Pattern来实现，这个模式的特点在于，定义了一组实现，将每一个实现都封装起来，并且使他们之间可以相互转换


运行时多态

```cpp

// Strategy接口，会有多种实现
class Lock {
 public:
  virtual void lock() const = 0;
  virtual void unlock() const = 0;
};

// Context类，负责组装Strategy
class StrategizedLocking {
  public:
    StrategizedLocking(Lock& l) : lock_(l) {
      lock.lock();
    }

    ~StrategizedLocking() {
      lock.unlock();
    }
  private:
    Lock& lock_;
}

struct NullObjectMutex {
  void lock() {}
  void unlock() {}
};

class NoLock : public lock {
  mutable NullObjectMutex mutex_;
};

class ExclusiveLock : public lock {
  mutable std::mutex mutex_;
}

class SharedLock : public Lock {
  mutable std::shared_mutex shared_mutex_;
}
```

编译时多态

```cpp
template <typename LOCK>
class StrategizedLocking {
  public:
    StrategizedLocking(LOCK& l) : lock(l) {
      lock.lock();
    }

    ~StrategizedLocking() {
      lock.unlock();
    }
  private:
    LOCK& lock;
}
```

还有一种方式就是提供`_unlock`后缀的方法。



## Active Object
Active Object 设计模式的本质是解耦合方法的调用 (Method invocation) 与方法的执行 (Method execution)，方法调用发生在调用者线程上下文中，而方法的执行发生在独立于调用者线程的 Active Object 线程上下文中。并且重要的一点是，该方法与其它普通的对象成员方法对于调用者来说，没有什么特别的不同。从运行时的角度来看，这里涉及到两类线程，一个是调用者线程，另外一个是 Active Object 线程

```cpp
#include <algorithm>
#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>
#include <thread>
#include <utility>
#include <vector>

class IsPrime {
 public:
  std::pair<bool, int> operator()(int i) {
    for(int j = 2; j * j <= i; ++j) {
      if (i % j == 0) return std::make_pair(false, i);
    }
    return std::make_pair(true, i);
  }
};

class ActiveObject {
 public:
   std::future<std::pair<bool, int>> enqueueTask(int i) {
      IsPrime isPrime;
      std::packaged_task<std::pair<bool, int>(int)> newJob(isPrime);

      auto isPrimeFuture = newJob.get_future();
      auto pair = std::make_pair(std::move(newJob), i);

      {
        std::lock_guard<std::mutex> lockGuard(activationListMutex);
        activationList.push_back(std::move(pair));
      }

      return isPrimeFuture;
   }

   void run() {
    std::thread servant([this] {
      while(!isEmpty()) {
        auto myTask = dequeueTask();
        myTask.first(myTask.second);
      }
    });
    servant.join();
   }

 private:

   std::pair<std::packaged_task<std::pair<bool, int>(int)>, int> dequeueTask() {
    std::lock_guard<std::mutex> lockGuard(activationListMutex);
    auto myTask = std::move(activationList.front());
    activationList.pop_front();
    return myTask;
   }

   bool isEmpty() {
    std::lock_guard<std::mutex> lockGuard(activationListMutex);
    auto empty = activationList.empty();
    return empty;
   }

   std::deque<std::pair<std::packaged_task<std::pair<bool, int>(int)>, int>> activationList;
   std::mutex activationListMutex;
};

std::vector<int> getRandNumbers(int number) {
  std::vector<int> numbers;
  for(long long i = 0; i < number; ++i) numbers.push_back(i);

  return numbers;
}


std::future<std::vector<std::future<std::pair<bool, int>>>> getFutures(ActiveObject& activeObject, int numberPrimes) {
  return std::async([&activeObject, numberPrimes] {
    std::vector<std::future<std::pair<bool, int>>> futures;
    auto randNumbers = getRandNumbers(numberPrimes);
    for(auto numb : randNumbers) {
      futures.push_back(activeObject.enqueueTask(numb));
    }
    return futures;
  });
}

int main() {
  std::cout << "starting........" << std::endl;
  ActiveObject activeObject;

  auto client1 = getFutures(activeObject, 1998);
  auto client2 = getFutures(activeObject, 2003);
  auto client3 = getFutures(activeObject, 2011);
  auto client4 = getFutures(activeObject, 2014);
  auto client5 = getFutures(activeObject, 2017);

  auto futures = client1.get();
  auto futures2 = client2.get();
  auto futures3 = client3.get();
  auto futures4 = client4.get();
  auto futures5 = client5.get();

  futures.insert(futures.end(), std::make_move_iterator(futures2.begin()), std::make_move_iterator(futures2.end()));
  futures.insert(futures.end(), std::make_move_iterator(futures3.begin()), std::make_move_iterator(futures3.end()));
  futures.insert(futures.end(), std::make_move_iterator(futures4.begin()), std::make_move_iterator(futures4.end()));
  futures.insert(futures.end(), std::make_move_iterator(futures5.begin()), std::make_move_iterator(futures5.end()));

  activeObject.run();
  std::vector<std::pair<bool, int>> futResults;
  futResults.reserve(futures.size());

  for(auto& fut : futures) futResults.push_back(fut.get());
  std::sort(futResults.begin(), futResults.end());

  auto prIt = std::find_if(futResults.begin(), futResults.end(), [](std::pair<bool, int> pa) { return pa.first == true; });

  std::cout << "Number primes: " << std::distance(prIt, futResults.end()) << std::endl;
  std::cout << "Primes: " << std::endl;

  std::for_each(prIt, futResults.end(), [](auto p) { std::cout << p.second << " ";});

  std::cout << "\n\n";

  std::cout << "Number on primes: " << std::distance(futResults.begin(), prIt) << std::endl;
  std::cout << "No primes: " << std::endl;

  std::for_each(futResults.begin(), prIt, [](auto p) { std::cout << p.second << " ";});

  std::cout << std::endl;
  return 0;
}
```

## API设计

* 永远不要返回私有数据成员的非const指针或引用，这会破坏封装性。
* 将私有功能声明在.cc文件中的静态函数，而不要将其作为私有方法暴露在公开的头文件中
* 剃刀原理，若无必要，勿增实体
* 疑惑之时，果断弃之，精简API中公有的类和函数，当不确定是否需要某个接口时，就不要提供此接口，该建议违背了API设计人员的良好意愿，因为工程师容易被解决方案的通用性和灵活性诱惑，所以可能情不自禁的为API
  增加抽象层次或通用性，盼望着他们将来可能被用到。工程师应该抵制这种诱惑，原因如下:
  1. 你想要添加的通用性可能永远用不到
  2. 如果某天用到了想要添加的通用性，那时你可能已经掌握了更多API设计的知识，并可能有了与最初设想方案不用的解决方案。
  3. 如果你确实需要添加新功能，那么简单的API比复杂的API更容易添加新功能

* 谨慎添加虚函数
  1. 虚函数的调用必须在运行时查询函数表来决定，这就使得虚函数的调用比非虚函数调用的慢。
  2. 使用虚函数一般需要维护指向虚函数表的指针，进而增加了对象的大小。
  3. 添加、重排、移除虚函数会破坏二进制兼容性

* NVI(Non-Virtual interface idiom)，接口是非虚的，将虚接口声明为私有的。然后在内部调用虚接口。

* API应该符合正交原则，意味着函数没有副作用。
  设计正交API时需要铭记如下两个重要因素：
  1. 减少冗余
  2. 增加独立性

* 除非确实需要`#include`类的完整定义，否则应该为类使用前置声明。
* 不要将平台相关的`#if`或`#ifdef`语句放在公共的API中，因为这些语句暴露了实现细节，并使API因平台而异。
* 优秀的API表现为松耦合和高内聚
* 使用Pimpl惯用法，将实现细节从公有头文件中分离出来，而且要采用私有内嵌实现类，只有在.cc文件中的其他类需要访问Impl成员时才应采用公有内嵌类。
  Pimpl带来了优点:

  1. 信息隐藏
  2. 降低耦合
  3. 加速编译
  4. 更好的二进制兼容性
  5. 惰性分配

  带来的缺点则是:

  1. 必须通过指针间接访问所有成员变量，可能引入性能冲击
  2. 编译器将不能捕获const方法中对成员变量的修改

* 任何复杂的系统都有两种层次化视角
  1. 对象层次结构，描述系统中不同的对象如何合作，这表现为基于对象之间的 "part of" 这种关系的结构化分组。
  2. 类层次结构 描述共存于关联对象之间的公共结构和行为，可以认为是对象层次结构之间的一种 "is a" 的关系
* 名词来表示对象、动词表示函数名、而形容词和所有格名词表示属性
* 对于单参数构造函数使用`explicit`关键字以避免意外的类型转换
* Liskov替换原则指出，在不修改任何行为的情况下用派生类替换基类，这应该总是可行的
* 开放-封闭原则，该原则指出，类的目标应该是为了扩展而开放，为修改而关闭。
* 接口往往使用对象模型中的形容词来表示
* 不同的编译单元中非局部静态对象的初始化顺序是未定义的
* 使用一致的函数命名和参数顺序
* 使用枚举类型代替布尔类型，提高代码的可读性
* 基于最小化核心API，以独立的模块或库的形式构建便捷API
* WillChange、DidChange 通知是在事件之前还是之后发送
* 回调是一种用于打破循环依赖的常用技术
* Big three原则，意思就是析构函数、复制构造函数和赋值操作符这三个成员函数始终应该在一起出现，如果定义了其中一个，通常也需要定义另外两个
* `-fno-implicit-templates` 关闭隐式实例化，尽量选择显示模版实例化，这样做可以隐藏私有细节，并降低构建时间，C++11中可以extern关键字来阻止编译器在当前
  编译单元中实例化一个模版