## 语法基础

* Format格式化输出

```rust
/// nothing ⇒ Display
/// ? ⇒ Debug
/// x? ⇒ Debug with lower-case hexadecimal integers
/// X? ⇒ Debug with upper-case hexadecimal integers
/// o ⇒ Octal
/// x ⇒ LowerHex
/// X ⇒ UpperHex
/// p ⇒ Pointer
/// b ⇒ Binary
/// e ⇒ LowerExp
/// E ⇒ UpperExp
```
* 全局常量

1. `const` 不可改变的值（常用类型)
2. `static` 在`'static` 生命周期内可能发生改变的变量
3. 常量的名字必须是全大写，如果是多个单词组成就用下划线来连接，必须指明类型

> static variable `mAX_HEALTH` should have an upper case name such as `M_AX_HEALTH`

* 变量名前缀使用`_`表示这是一个`unused variable`。

* Rust中变量是可以隐藏的，再次声明和定义变量的时候会覆盖之前声明定义的变量。

* Rust中的类型之间是不允许隐式转换的，需要通过as来显示的进行类型转换。

* Rust中一个{}就是一个块作用域，不同块作用域中的相同变量名之间会覆盖，优先查找当前块作用域中的变量。

* Rust中通过as来进行类型转换

* 当一个变量存在引用的时候，是不能对其进行move的，这会导致引用失效

* Rust中变量默认不可变，不能重新绑定，如果希望变量可以修改需要添加mut前缀

> cannot assign twice to immutable variable

* 通过type可以给类型取别名，但是别名的名字必须是`CamelCase`驼峰格式否则编译器会产生警告，通过`#[allow(non_camel_case_types)]`可以关闭编译器的警告

* rust使用()来表示空值，大小是0，是unit type的唯一值，用来表示一个函数或者一个表达式没有返回任何值，()和其他语言中的null不一样，()不是值。

* `Vec<T>`相当于C++中的vector，其索引必须是usize类型。

* `assert!`、`assert_eq!`、`unimplemented!()`、`unreachable!`、`panic!`、`format!`

* `self`(this指针)、`Self`(类型本身)

* 下划线开头的变量会当场析构

* `std::mem::drop` 可以立即析构一个对象语义的变量(本质上是将其移动到函数作用域内)

* Closure 默认是引用捕获，可以通过move来做take Ownership，但是实际上上并不一定是move，有可能是copy
* Closure Traits

```rust
pub trait Fn<Args> : FnMut<Args> {
    extern "rust-call"
      fn call(&self, args: Args) -> Self::Output;
}
pub trait FnMut<Args> : FnOnce<Args> {
    extern "rust-call"
      fn call_mut(&mut self, args: Args) -> Self::Output;
}
pub trait FnOnce<Args> {
    type Output;
    extern "rust-call"
      fn call_once(self, args: Args) -> Self::Output;
}
```
* try!，遇到错误的时候提前return

```rust
let socket1: TcpStream = try!(TcpStream::connect("127.0.0.1:8000"));
// Is equivalent to...
let maybe_socket: Result<TcpStream> =
    TcpStream::connect("127.0.0.1:8000");
let socket2: TcpStream =
    match maybe_socket {
        Ok(val) => val,
        Err(err) => { return Err(err) }
    };
```

* Option::unwrap，方便了对Option 返回值的处理，但是如果是None会导致执行`panic`

```rust
fn unwrap<T>(&self) -> T {
    match *self {
        None => panic!("Called `Option::unwrap()` on a `None` value"),
        Some(value) => value,
    }
}

```

* 为什么&String是&str?，因为String实现了Deref trait

```rust
pub trait Deref {
    type Target: ?Sized;
    fn deref(&self) -> &Self::Target;
}
```

* String转&str，不能直接使用str，必须是&str，str是一个unsized的类型

```rust
let addr = "192.168.0.1:3000".to_string();
TcpStream::connect(&*addr);
```

* `#[derive(...)]`告诉编译器插入trait的默认实现

* Sized(类型必有有常量大小，在编译时)，默认所有的类型都是Sized的，?Sized(类型可能有大小) example: Box<T> allows T: ?Sized.

* Trait object对象安全，只有是对象安全的trait才可以做动态多态，否则只能static dispatch

```rust
A trait is object-safe if:
1. It does not require that Self: Sized
2. Its methods must not use Self
3. Its methods must not have any type parameters
4. Its methods do not require that Self: Sized
```

* 关联类型，和trait相关的类型

```rust
// 避免这样，N和E在这里不应该是通用类型，而是一个和Graph相关的类型。
trait Graph<N, E> {
    fn edges(&self, &N) -> Vec<E>;
    // etc
}

trait Graph {
  type N;
  type E;
  fn edges(&self, &Self::N) -> Vec<Self::E>;
}
impl Graph for MyGraph {
  type N = MyNode;
  type E = MyEdge;
  fn edges(&self, n: &MyNode) -> Vec<MyEdge> { /*...*/ }
}
```

* Rust中的范型可以指定满足那些Trait和C++20中的Concept一致

```rust
// T需要有Clone trait
fn cloning_machine<T: Clone>(t: T) -> (T, T) {
    (t.clone(), t.clone())
}
// 痛where语句来选择
fn cloning_machine_2<T>(t: T) -> (T, T)
        where T: Clone {
    (t.clone(), t.clone())
}
// 满足多个trait
fn clone_and_compare<T: Clone + Ord>(t1: T, t2: T) -> bool {
   t1.clone() > t2.clone()
}
```

* match模式匹配的时候默认是值拷贝的方式，可以通过`ref`来创建引用或者是通过`ref mut`来创建可变的引用

```rust
  // 相应地，定义两个非引用的值，通过 `ref` 和 `mut` 可以取得引用。
  let value = 5;
  // 使用 `ref` 关键字来创建引用。
  match value {
      ref r => println!("Got a reference to a value: {:?}", r),
  }
```

* 如果match是对一个引用进行模式匹配的化可以通过`*`来解引用，这样就可以通过值拷贝的方式来进行模式解耦

```rust
  // 获得一个 `i32` 类型的引用。`&` 表示获取一个引用。
  let reference = &4;

  // 为了避免 `&` 的使用，需要在匹配前解引用。
  match *reference {
      val => println!("Got a value via dereferencing: {:?}", val),
  }
```

* 如果match是对一个引用进行模式匹配的化，可以使用值的方式、`ref`的方式来，`&`的方式来进行匹配

```rust
  let reference = &4;

  match reference {
    // val => println!("xxxx"),
    // ref val => println!("xxxxx"),
    &val => println!("Got a value via destructuring: {:?}", val),
  }

```

* 模式匹配的时候可以通过`..`来忽略某些变量

* `match`匹配的时候，还可以额外添加`guard`来过滤

```rust
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

```rust
	let age: u32 = 15;
  match age {
    0 => println!("I'm not born yet I guess"),
    n @ 1 ... 12 => println!("I'm a child of age {:?}", n),
    n @ 13 ... 19 => println!("I'm a teen of age {:?}", n),
    n => println!("I'm an old person of age {:?}", n),
  }
```

* 使用`if let`可以用来进行模式匹配，避免使用match导致代码冗余

```rust
let number = Some(7);

if let Some(i) ==number {
  //.....
} else {
  //.....
}
```

* 指定闭包进行变量捕获的方式

```rust
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

* borrowing

1. When data is immutably borrowed, it also freezes. Frozen data can't be modified via the original object until all references to it go out of scope:
2. Data can be immutably borrowed any number of times, but while immutably borrowed, the original data can't be mutably borrowed.
On the other hand, only one mutable borrow is allowed at a time. The original data can be borrowed again only after the mutable reference goes out of scope

```rust
fn main() {
    let mut _mutable_integer = 7i32;

    {
        // Borrow `_mutable_integer`
        let _large_integer = &_mutable_integer;

        // Error! `_mutable_integer` is frozen in this scope
        _mutable_integer = 50;
        // FIXME ^ Comment out this line

        // `_large_integer` goes out of scope
    }

    // Ok! `_mutable_integer` is not frozen in this scope
    _mutable_integer = 3;
}
```

* Function with lifetimes

function signatures with lifetimes have a few constraints:

1. any reference must have an annotated lifetime.
2. any reference being returned must have the same lifetime as an input or be static.

* Diverging functions 偏离函数就是返回值是`!`的函数
* `#![no_std]` 禁用rust std标准库
* `#[panic_handler]` 自定义panic hook函数，标准库提供了默认版本

```rust
use core::panic::PanicInfo;

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
  //....
}
```

* `-C panic|unwind` 通过编译选项来关闭(通过panic来替代)或者启用栈解旋，或者通过cargo来开启

```rust
[profile.dev]
panic = "abort"

[profile.release]
panic = "abort"
```

* C运行时启动后调用crt0，然后调用rust的运行时(标记为start language item)，rust运行时最后再调用main函数
* `no_mangle`关闭name mangling
* `-C link-arg=-nostartfiles` 链接的时候不链接libc运行时
* `#![no_main]`覆盖entry point，Linux下可以用如下凡事定义新的entry point

```rust
#[no_mangle]
// extern "C" 用于告诉编译器，按照C的调用约定来进行函数调用
pub extern "C" fn _start() -> ! {
    loop {}
}
```

* `--target` 指定编译的平台，`CPU架构`、`vendor`、`OS`、`ABI`等，下面是一个target的例子。

```rust
{
    "llvm-target": "x86_64-unknown-none",
    "data-layout": "e-m:e-i64:64-f80:128-n8:16:32:64-S128",
    "arch": "x86_64",
    "target-endian": "little",
    "target-pointer-width": "64",
    "target-c-int-width": "32",
    "os": "none",
    "executables": true,
}
```

* `Copy trait`本质上实现是添加了 `#[lang = "copy"]`编译器属性
* `#[repr(u8)]` 指定enum使用`u8`类型来存储
* `eh_personality`用于实现栈解旋，是一个language item，
* `#[derive(Debug, Clone, Copy, PartialEq, Eq)]` 开启Copy语义
* `repr(C)` 让rust中的struct字段顺序和C中的struct一致
* `core::fmt::Write` Traits 需要实现`fn write_str(&mut self, s: &str) -> fmt::Result`方法，可以调用`write!`宏，写入格式化字符串
* `#[macro_export]`把定义的宏暴露出去，所有的crate都可以使用，属于root namesapce
* `#[doc(hidden)]` 不给public的function生成文档信息
* cargo中可以定义section `dev-dependencies`用来只在开发截断才依赖的crate
* rust条件编译来确定属性`#![cfg_attr(not(test), no_main)]`、`#[cfg(not(test))]`、`#![cfg_attr(test, allow(unused_imports))]`等
* 数组构造的时候要求类型是Copy语义的`array construction in Rust requires that the contained type is Copy`
* 通过`array_init`可以让数组构造不是Copy语义的类型。
* `volatile`crate库用于避免编译器的优化，对于一段内存，如果只有写没有读，编译器可能会把写操作给优化掉。
* `Box::leak`用于将消费Box::new创建出来的智能指针，并返回`'a mut`指针
* 数组的构造需要其类型是Copy语义的，为了解决这个问题需要用到`array-init` crate

```rust
// Volatile 是Non-Copy的类型
struct Buffer {
    chars: [[Volatile<ScreenChar>; BUFFER_WIDTH]; BUFFER_HEIGHT],
}

fn construct_buffer() -> Buffer {
  use array_init::array_init;

  Buffer {
      chars: array_init(|_| array_init(|_| Volatile::new(empty_char()))),
  }
}
```

* `#[repr(transparent)]`确保struct和内部的类型是一致的内存布局

```rust
// 确保ColorCode和u8是一样的内存布局
#[repr(transparent)]
struct ColorCode(u8);
```

* 测试panic的场景

```rust
    #[should_panic]
    // or use:
    #[should_panic(expected = "Some message")]
    fn it_works2() {
    }
```

* 输出测试输出内容

1. `cargo test -- --color always --nocapture`
2. `cargo test -- --nocapture`
3. `set RUST_TEST_NOCAPTURE=1`

* 自定义ERROR

```rust
#[derive(Debug, PartialEq)]
pub enum UUIDError {
    InvalidChar(char),
    InvalidGroupCount(usize),
    InvalidLength(usize),
    InvalidGroupLength(u8),
}

type Result<T> = result::Result<T, UUIDError>;
```

* `mem::replace`

如何将一个对象的值move到其他地方，然后重新赋值呢?

```rust
pub struct List {
    head: Link,
}

enum Link {
    Empty,
    More(Box<Node>),
}

struct Node {
    elem: i32,
    next: Link,
}


pub fn push(&mut self, elem: i32) {
    let new_node = Box::new(Node {
        elem: elem,
        next: self.head,  // 这里将self.head移动到next
    });

    // 这里进行了重新赋值，编译器会警告，因为使用了一个已经被moved的变量
    self.head = Link::More(new_node);
}

// mem::replace可以一次性完成这个交换赋值的动作，也可以先临时替换为一个中间值，然后再赋值

pub fn push(&mut self, elem: i32) {
    let new_node = Box::new(Node {
        elem: elem,
        // 先替换为一个中间值
        next: mem::replace(&mut self.head, Link::Empty),
    });

    self.head = Link::More(new_node);
}
```

Option的take方法其实就是`mem::replace(&mut variable, None)`;

* Option map方法

map方法等同于下面这种形式，通过模式匹配Option拿到其中的元素，传递到map中的lambda方法，
对于Lambda方法中返回的元素通过Some进行包装。

```rust
match option { None => None, Some(x) => Some(y) }
```

* `Option<&mut value>` 调用map传递的value就是mut的

* 如何创建自定义迭代器

* lifetime-elision

  * Each elided lifetime in input position becomes a distinct lifetime parameter.
  * If there is exactly one input lifetime position (elided or not), that lifetime is assigned to all elided output lifetimes.
  * If there are multiple input lifetime positions, but one of them is &self or &mut self, the lifetime of self is assigned to all elided output lifetimes.
  * Otherwise, it is an error to elide an output lifetime.

* mut修饰位置不同，其含义不同

```rust
fn main() {
  let mut var = 0_i32;  // mut修饰变量名，表示这个变量是可以重新指向新的变量的。
  {
    let p1 = &mut var;  // mut 修饰的是借用指针&，表示被指向的对象是可以被修改的
    *p1 = 1;
  }

  {
    let temp = 2_i32;
    let mut p2 = &var;  // p2可以重新指向，但是无法通过p2来更改var的值
    p2 = &temp
  }

  {
    let mut temp = 3_i32;
    let mut p3 = &mut var;  // 既可以重新指向，也可以修改var的值
    *p3 = 3;
    p3 = &mut temp
  }
}
```

* 共享不可变、可变不共享，唯一修改权原则

* error handling 针对Option或Result处理方式相同
  1. `panic!`
  2. `unwrap()`等同于match的时候，遇到error或None触发`panic!`

```rust
impl<T> Option<T> {
  fn unwrap(self) -> T {
    match self {
        Option::Some(val) => val,
        Option::None =>
          panic!("called `Option::unwrap()` on a `None` value"),
    }
  }
}
```

  3. `unwrap_or`

```rust
fn unwrap_or<T>(option: Option<T>, default: T) -> T {
    match option {
        None => default,
        Some(value) => value,
    }
}
```
  4. `map` 将Option解开，如果不是None就回调function，最后将返回值包装成Option

```rust
fn map<F, T, A>(option: Option<T>, f: F) -> Option<A> where F: FnOnce(T) -> A {
    match option {
        None => None,
        Some(value) => Some(f(value)),
    }
}
```

  5. `and_then` 这个和map相同，只是要求function的返回值是Option，不对其返回值封装成Option

```rust
fn and_then<F, T, A>(option: Option<T>, f: F) -> Option<A>
        where F: FnOnce(T) -> Option<A> {
    match option {
        None => None,
        Some(value) => f(value),
    }
}
```

  6. `unwrap_or_else` 如果是None就回调function，否则就直接返回Option的值

  7. Result type alias idiom

```rust
use std::num::ParseIntError;
use std::result;

type Result<T> = result::Result<T, ParseIntError>;

fn double_number(number_str: &str) -> Result<i32> {
    unimplemented!();
}
```

  8. `ok_or` 将Option转换为Result

```rust
fn ok_or<T, E>(option: Option<T>, err: E) -> Result<T, E> {
    match option {
        Some(val) => Ok(val),
        None => Err(err),
    }
}
```

  9. `map_err` Maps a `Result<T, E>` to `Result<T, F>`，如果是Ok就原封不动返回，否则就将error值传递给fucntion，返回另一种类型，最后将结果包装成Result

  10. `try!` or `?`

```rust
macro_rules! try {
    ($e:expr) => (match $e {
        Ok(val) => val,
        Err(err) => return Err(err),
    });
}
```

  11. `std::error::Error` trait，自定义错误类型，需要实现这个trait

```rust
use std::fmt::{Debug, Display};

trait Error: Debug + Display {
  /// A short description of the error.
  fn description(&self) -> &str;

  /// The lower level cause of this error, if any.
  fn cause(&self) -> Option<&Error> { None }
}
```

  12. `Box<Error>` 使用这个错误类型，而不是具体的类型。

  13. `expect`  如果是None就`panic!`，并输出一段自定义的错误消息。

  14. `std::convert::From` 自定义类型转换，可以将自定义的错误类型转换为预期的标准错误类型

```rust
trait From<T> {
    fn from(T) -> Self;
}
```

  15. `failure::Error` `failure` crate，类似于`Box<Error>`，但是这个crate额外可以打印backtraces，以及好的向下转型的支持。

## Link
* [Cfg Test and Cargo Test a Missing Information](https://freyskeyd.fr/cfg-test-and-cargo-test-a-missing-information/)
* [System V ABI read zone](https://os.phil-opp.com/red-zone/)
* [disbale SIMD](https://os.phil-opp.com/disable-simd/)
* [Too Many Linked Lists](https://rust-unofficial.github.io/too-many-lists/)