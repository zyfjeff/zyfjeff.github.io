## 语法基础

* 常量

1. `const` 不可改变的值（常用类型)
2. `static` 在`'static` 生命周期内可能发生改变的变量

* 变量名前缀使用`_`表示这是一个`unused variable`。

* Rust中变量是可以隐藏的，再次声明和定义变量的时候会覆盖之前声明定义的变量。

* Rust中可以声明一个变量但是不进行初始化，一般不推荐，如果声明的时候不初始化可能会导致使用未初始化的变量，这个会被编译器检测到。

* Rust中通过as来进行类型转换

* 通过type可以给类型取别名，但是别名的名字必须是`CamelCase`驼峰格式否则编译器会产生警告，通过`#[allow(non_camel_case_types)]`可以关闭编译器的警告

* match模式匹配的时候默认是值拷贝的方式，可以通过`ref`来创建引用或者是通过`ref mut`来创建可变的引用

```
  // 相应地，定义两个非引用的值，通过 `ref` 和 `mut` 可以取得引用。
  let value = 5;
  // 使用 `ref` 关键字来创建引用。
  match value {
      ref r => println!("Got a reference to a value: {:?}", r),
  }
```

* 如果match是对一个引用进行模式匹配的化可以通过`*`来解引用，这样就可以通过值拷贝的方式来进行模式解耦

```
  // 获得一个 `i32` 类型的引用。`&` 表示获取一个引用。
  let reference = &4;

  // 为了避免 `&` 的使用，需要在匹配前解引用。
  match *reference {
      val => println!("Got a value via dereferencing: {:?}", val),
  }
```

* 如果match是对一个引用进行模式匹配的化，可以使用值的方式、`ref`的方式来，`&`的方式来进行匹配

```
  let reference = &4;

  match reference {
    // val => println!("xxxx"),
    // ref val => println!("xxxxx"),
    &val => println!("Got a value via destructuring: {:?}", val),
  }
    
```

* 模式匹配的时候可以通过`..`来忽略某些变量

* `match`匹配的时候，还可以额外添加`guard`来过滤

```
  let pair = (2, -2);
  println!("Tell me about {:?}", pair);
  match pair {
    (x, y) if x == y => println!("These are twins"),
    (x, y) if x + y == 0 => println!("Antimatter, kaboom!"),
    (x, _) if x % 2 == 1 => println!("The first one is odd"),
    _ => println!("No correlation..."),
  }
```

* `match`匹配的时候，如果匹配的是一系列的值时可以通过`@`来绑定匹配到的一系列值

```
	let age: u32 = 15;
  match age {
    0 => println!("I'm not born yet I guess"),
    n @ 1 ... 12 => println!("I'm a child of age {:?}", n),
    n @ 13 ... 19 => println!("I'm a teen of age {:?}", n),
    n => println!("I'm an old person of age {:?}", n),
  }
```

* 使用`if let`可以用来进行模式匹配，避免使用match导致代码冗余

```
let number = Some(7);

if let Some(i) ==number {
  //.....
} else {
  //.....
}
```

* 指定闭包进行变量捕获的方式

```
Fn：闭包需要通过引用（&T）捕获
FnMut：闭包需要通过可变引用（&mut T）捕获
FnOnce：闭包需要通过值（T）捕获

fn apply<F>(f: F) where
    // 闭包没有输入值和返回值。
    F: FnOnce() {
    // ^ 试一试：将 `FnOnce` 换成 `Fn` 或 `FnMut`。

    f();
}

// 使用闭包并返回一个 `i32` 整型的函数。
fn apply_to_3<F>(f: F) -> i32 where
// 闭包处理一个 `i32` 整型并返回一个 `i32` 整型。
    F: Fn(i32) -> i32 {

    f(3)
}
```
* Rust范型的使用

```
fn call_me<F>(f: F) where
  F: FnOnce() {
  f()
}

fn call_me<F: Fn()>(f: F) {
      f()
}
```
