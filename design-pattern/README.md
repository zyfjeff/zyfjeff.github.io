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

* PIMPL = Pointer to an IMPLementation


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


## API设计

1. 正交原则
2. 使用一致的函数命名和参数顺序
3. 使用枚举类型代替布尔类型，提高代码的可读性
4. 基于最小化核心API，以独立的模块或库的形式构建便捷API
5. 剃刀原理，若无必要，勿增实体
6. WillChange、DidChange 通知是在事件之前还是之后发送
7. 回调是一种用于打破循环依赖的常用技术