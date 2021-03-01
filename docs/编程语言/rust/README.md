## 语法基础

* 常见的属性
  1. `#[allow(non_camel_case_types)]`   // 允许非驼峰命名，默认情况下，类型名需要是驼峰类型，否则会有警告
  2. `#![allow(overflowing_literals)]`

* (TODO)为什么`Cell`要求类型必须是Copy的，而不是Clone?

* (TODO)Rust中默认的“取引用”、“解引用”操作是互补抵消的关系， 互为逆运算。但是，在Rust中，只允许自定义“解引用”，不允许自定义“取引用”。
  如果类型有自定义“解引用”，那么对它执行“解引用”和“取引用”就不再是互补抵消的结果了。先`&`后`*`以及先`*`后`&`的结果是不同的。

* 函数参数可以直接解构作为一个个独立参数

```rust
struct T {
    item1: char,
    item2: bool,
}

fn test(
    T {
        item1: arg1,
        item2: arg2,
    }: T
) {
    println!("{} {}", arg1, arg2);
}
```

* Closure 默认是引用捕获，可以通过move来做take Ownership，但是实际上上并不一定是move，有可能是copy

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
// where语句来选择
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

1. When data is immutably borrowed, it also freezes.
Frozen data can't be modified via the original object until all references to it go out of scope:
2. Data can be immutably borrowed any number of times, but while immutably borrowed,
the original data can't be mutably borrowed. On the other hand, only one mutable borrow is allowed at a time.
The original data can be borrowed again only after the mutable reference goes out of scope

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

```rust
fn guess(n: i32) -> bool {
    if n < 1 || n > 10 {
        panic!("Invalid number: {}", n);
    }

    n == 5
}

fn main() {
    guess(11);
}
```

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

use std::env;

fn main() {
  let mut argv = env::args();
  let arg: String = argv.nth(1).unwrap();
  let n: i32 = arg.parse().unwrap();
  println!("{}", 2 * n);
}
```

  3. `unwrap_or` 从Option中获取值，如果是None就返回指定的默认值

```rust
fn unwrap_or<T>(option: Option<T>, default: T) -> T {
    match option {
        None => default,
        Some(value) => value,
    }
}

use std::env;

fn main() {
  let mut argv = env::args();
  let arg: String = argv.nth(1).unwrap_or(String::from("unknow number"));
  let n: i32 = arg.parse().unwrap_or(2);
  println!("arg: {}, number: {}", arg, 2 * n);
}
```

  4. `map` 将Option解开，如果不是None就回调function，最后将返回值包装成Option，本质上是一种Option转换成另外一种Option

```rust
fn map<F, T, A>(option: Option<T>, f: F) -> Option<A> where F: FnOnce(T) -> A {
    match option {
        None => None,
        Some(value) => Some(f(value)),
    }
}

// Searches `haystack` for the Unicode character `needle`. If one is found, the
// byte offset of the character is returned. Otherwise, `None` is returned.
fn find(haystack: &str, needle: char) -> Option<usize> {
    for (offset, c) in haystack.char_indices() {
        if c == needle {
            return Some(offset);
        }
    }
    None
}

fn main() {
  let m = find("findstring", 'd');
  println!("size: {}", m.map(|x| (x + 100).to_string() + "oo").unwrap());
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

// Searches `haystack` for the Unicode character `needle`. If one is found, the
// byte offset of the character is returned. Otherwise, `None` is returned.
fn find(haystack: &str, needle: char) -> Option<usize> {
    for (offset, c) in haystack.char_indices() {
        if c == needle {
            return Some(offset);
        }
    }
    None
}

fn main() {
  let m = find("findstring", 'd');
  println!("size: {}", m.and_then(|x| Some(x.to_string() + "test")).unwrap());
}
```

  6. `unwrap_or_else` 如果是None就回调function，这个function会返回一个值，否则就直接返回Option的值

```rust
fn main() {
  let mut argv = env::args();
  let arg: String = argv.nth(1).unwrap_or_else(|| "test".to_owned());
  println!("arg: {}", arg);
}
```

  7. Result type alias idiom

```rust
use std::num::ParseIntError;
use std::result;

type Result<T> = result::Result<T, ParseIntError>;

fn double_number(number_str: &str) -> Result<i32> {
    unimplemented!();
}

fn double_number(number_str: &str) -> i32 {
    2 * number_str.parse::<i32>().unwrap()
}

fn main() {
    let n: i32 = double_number("10");
    assert_eq!(n, 20);
}

// 通过map来转换
fn double_number(number_str: &str) -> i32 {
    2 * number_str.parse::<i32>().map(|number| number + 10).unwrap()
}

fn main() {
    let n: i32 = double_number("10");
    assert_eq!(n, 40);
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

use std::env;

fn double_arg(mut argv: env::Args) -> Result<i32, String> {
    argv.nth(1)
        .ok_or("Please give at least one argument".to_owned())
        .and_then(|arg| arg.parse::<i32>().map_err(|err| err.to_string()))
}

fn main() {
    match double_arg(env::args()) {
        Ok(n) => println!("{}", n),
        Err(err) => println!("Error: {}", err),
    }
}
```

  9. `map_err` Maps a `Result<T, E>` to `Result<T, F>`，如果是Ok就原封不动返回，否则就将error值传递给fucntion，返回另一种类型，最后将结果包装成Result

```rust
pub fn map_err<F, O>(self, op: O) -> Result<T, F>
where
    O: FnOnce(E) -> F,

fn stringify(x: u32) -> String { format!("error code: {}", x) }

fn main() {
  let x: Result<u32, u32> = Ok(2);
  assert_eq!(x.map_err(stringify), Ok(2));

  let x: Result<u32, u32> = Err(13);
  assert_eq!(x.map_err(stringify), Err("error code: 13".to_string()));
}

```

  10. `try!` or `?`

```rust
macro_rules! try {
    ($e:expr) => (match $e {
        Ok(val) => val,
        Err(err) => return Err(err),
    });
}

use std::fs::File;
use std::io::Read;
use std::path::Path;

fn file_double<P: AsRef<Path>>(file_path: P) -> Result<i32, String> {
    let mut file = File::open(file_path).map_err(|e| e.to_string())?;
    let mut contents = String::new();
    file.read_to_string(&mut contents).map_err(|e| e.to_string())?;
    let n = contents.trim().parse::<i32>().map_err(|e| e.to_string())?;
    Ok(2 * n)
}

fn main() {
    match file_double("foobar") {
        Ok(n) => println!("{}", n),
        Err(err) => println!("Error: {}", err),
    }
}

use std::fs::File;
use std::io::Read;
use std::path::Path;

fn file_double<P: AsRef<Path>>(file_path: P) -> Result<i32, String> {
    let mut file = try!(File::open(file_path).map_err(|e| e.to_string()));
    let mut contents = String::new();
    try!(file.read_to_string(&mut contents).map_err(|e| e.to_string()));
    let n = try!(contents.trim().parse::<i32>().map_err(|e| e.to_string()));
    Ok(2 * n)
}

fn main() {
    match file_double("foobar") {
        Ok(n) => println!("{}", n),
        Err(err) => println!("Error: {}", err),
    }
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

  12. `Box<dyn Error>` 使用这个错误类型，而不是具体的类型，但是不能向下转型，没办法拿到进一步的错误信息

```rust
use std::error::Error;
use std::fs::File;
use std::io::Read;
use std::path::Path;

// 任何类型的Error都可以向上转型为Box<dyn Error>
fn file_double<P: AsRef<Path>>(file_path: P) -> Result<i32, Box<dyn Error>> {
    let mut file = File::open(file_path)?;
    let mut contents = String::new();
    file.read_to_string(&mut contents)?;
    let n = contents.trim().parse::<i32>()?;
    Ok(2 * n)
}

fn main() {
  match file_double("foobar") {
    Ok(n) => println!("{}", n),
    Err(err) => println!("Error: {}", err),
  }
}
```

  13. `expect`  如果是None就`panic!`，并输出一段自定义的错误消息。

  14. `std::convert::From` 自定义类型转换，可以将自定义的错误类型转换为预期的标准错误类型

```rust
trait From<T> {
    fn from(T) -> Self;
}
```

  15. `failure::Error` `failure` crate，类似于`Box<Error>`，但是这个crate额外可以打印backtraces，以及好的向下转型的支持。

  16. `map_or` 提供默认值，和map不同的时候，这个返回值和提供的默认值类型一致，而不是用`option`再包装一次

```rust
pub fn map_or<U, F>(self, default: U, f: F) -> U
where
    F: FnOnce(T) -> U,


let x = Some("foo");
assert_eq!(x.map_or(42, |v| v.len()), 3);

let x: Option<&str> = None;
assert_eq!(x.map_or(42, |v| v.len()), 42);
```

  17. `unwrap_err` 如果值是Ok则会panic，如果值是err则返回对应的Error值

```rust
pub fn unwrap_err(self) -> E

Unwraps a result, yielding the content of an Err.

Panics
Panics if the value is an Ok, with a custom panic message provided by the Ok's value.

use std::error::Error;
use std::fs;
use std::io;
use std::num;

fn main() {
  let io_err: io::Error = io::Error::last_os_error();
  let parse_err: num::ParseIntError = "2".parse::<i32>().unwrap_err();
  println!("{}", parse_err);
}
```

* 内部可变性

对象是不可变的，但是又需要某些情况下内部的一些字段是可变的，典型的像Rc、Mutex等，Rc在赋值的时候，希望内部的引用计数可以递增。但是Rc自身是不可变的。
RefCell和Cell可用于实现内部可变性，前者带有运行时的借用检查，后者没有，后者要求类型必须实现Cope trait，前者不要求。这两者都是非线程安全的。

线程安全版本的内部可变性
  1. Copy类型 `Atomic*类型` 可以实现线程安全版本的内部可变性。
  2. Non-Copy类型 `Arc<RwLock<_>>`

`Arc<Vec<RwLock<T>>>`和`Arc<RwLock<Vec<T>>>` 前者不能并发的操作`Vec`，但是可以并发的操作其值T，后者可以并发的操作Vec，但是无法并发操作T

内部可变性的内部实现依靠`UnsafeCell`，起内部实现依靠编译器

```rust
#[lang = "unsafe_cell"]
#[stable(feature = "rust1", since = "1.0.0")]
pub struct UnsafeCell<T: ?Sized> {
    value: T,
}

// 将不可变转换为可变
pub fn get(&self) -> *mut T {
    &self.value as *const T as *mut T
}

pub struct Cell<T> {
  value: UnsafeCell<T>,
}

pub fn get(&self) -> T {
  unsafe{ *self.value.get() }
}

pub fn set(&self, value: T) {
  unsafe {
      *self.value.get() = value;
  }
}

pub struct RefCell<T: ?Sized> {
    borrow: Cell<BorrowFlag>,
    value: UnsafeCell<T>,
}


pub fn borrow(&self) -> Ref<T> {
  match BorrowRef::new(&self.borrow) {
      Some(b) => Ref {
          value: unsafe { &*self.value.get() },
          borrow: b,
      },
      None => panic!("RefCell<T> already mutably borrowed"),
  }
}
```

* `&` vs `ref` in Rust patterns

match既可以操作引用也可以操作value

```rust
struct Foo(String);

fn main() {
    // 匹配引用，因此并没有发生move
    match foo {
        Foo(x) => println!("Matched! {}", x),
    };
    // 这里仍然还可以对foo进行操作。如果上面换成对value进行match，这里就会出现错误，显示foo的值已经被moved了
    println!("{}", foo.0);
}
```

  1. `&`作用于match一个引用的时候，用于match这个引用指向的对象

```rust
struct Foo(String);

fn main() {
    let foo = &Foo(String::from("test"));
    match foo {
        // 并不是match引用本身，而是引用指向的对象，foo本身是引用，其引用部分正好和pattern的`&`抵消，也就是所有Foo(x) 正好对于value部分
        &Foo(x) => println!("Matched! {}", x),
    };
    //这里就会出现错误，显示foo的值已经被moved了
    println!("{}", foo.0);
}
```
  2. ref 作用于match，避免value被move，而是使用引用获取match匹配的值

```rust
struct Foo(String);

fn main() {
    let foo = Foo(String::from("test"));
    // 尽管是对value进行match，但是通过ref关键字，使得可以通过引用过的方式访问match到的值
    match foo {
        Foo(ref x) => println!("Matched! {}", x),
    };
    println!("{}", foo.0);
}
```

* `std::mem::uninitialized`

1. 初始化数组的时候，如果数组中的元素不是Copy类型的数据，那么就需要使用到`std::mem::uninitialized`
2. 和libc交互的时候，声明一个变量，这个变量要被libc初始化

```rust
use std::mem;
use std::ptr;

// Only declare the array. This safely leaves it
// uninitialized in a way that Rust will track for us.
// However we can't initialize it element-by-element
// safely, and we can't use the `[value; 1000]`
// constructor because it only works with `Copy` data.
let mut data: [Vec<u32>; 1000];

unsafe {
    data = mem::uninitialized();
    for elem in &mut data[..] {
        ptr::write(elem, Vec::new());
    }
}
println!("{:?}", &data[0]);
```

```rust
impl PlatformInfo {
    pub fn new() -> io::Result<Self> {
        unsafe {
            let mut uts: utsname = mem::uninitialized();
            if uname(&mut uts) == 0 {
                Ok(Self { inner: uts })
            } else {
                Err(io::Error::last_os_error())
            }
        }
    }
}
```

## crate and mod

crate是Rust中独立的编译单元，每一个crate对应生成一个库，或者是可执行文件。
mod可以简单理解成命名空间，mod可以嵌套，还可以控制内部元素的可见性，mod可以循环引用，crate之间不可以，rust需要把整个crate全部load才能执行编译。

版本号的模糊匹配:

* ^1.2.3 ==> 1.2.3 <= version < 2.0.0 (默认行为，直接写一个固定版本号)
* ~1.2.3 ==> 1.2.3 <= version < 1.3.0
* 1.* ==> 1.0.0 <= version < 2.0.0

1. 可以指定官方仓库中的crate
2. 可以指定本地的crate
3. 可以指定git

`pub use` 重新导出模块中的元素，使得导出的元素，成为当前模块的一部分

指定方法的可见行:

* pub(crate)
* pub(in xxx_mod)
* pub(self) 或者 pub(in self) pub(super) 或者 pub(in super)



`#[non_exhaustive]` 新增enum成员的时候，不破坏兼容性



## FFI

Rust和C是ABI兼容的，但是需要满足一些条件

1. 使用`extern C`修饰的
2. 使用`#[no_mangle]`修饰的函数
3. 函数参数、返回值中使用的类型，必须是在Rust和C中具备同样的内存布局


* C调用Rut

Step1: 按照上述规则编写Rust接口，提供给C调用

```rust
#[no_mangle]
   pub extern "C" fn rust_capitalize(s: *mut c_char)
   {
       unsafe {
           let mut p = s as *mut u8;
           while *p != 0 {
               let ch = char::from(*p);
               if ch.is_ascii() {
                   let upper = ch.to_ascii_uppercase();
                   *p = upper as u8;
               }
               p = p.offset(1);
           }
} }
```

Step2: 编译成C的静态库

```
rustc --crate-type=staticlib capitalize.rs
```

Step3: 编写C代码开始调用

```C
#include <stdlib.h>
  #include <stdio.h>
  // declare
  extern void rust_capitalize(char *);
  int main() {
      char str[] = "hello world";
      rust_capitalize(str);
      printf("%s\n", str);
      return 0;
}
```

`gcc -o main main.c -L. -l:libcapitalize.a -lpthread -ldl`


* Rust调用C库

Step1: 编译好C的静态库

Step2: 在Rust中使用`#[link = "library_name"]`来找到对应的静态库，并通过`extern "C"`进行方法的声明

```rust
  use std::os::raw::c_int;
   #[link(name = "simple_math")]
   extern "C" {
       fn add_square(a: c_int, b: c_int) -> c_int;
   }
   fn main() {
       let r = unsafe { add_square(2, 2) };
       println!("{}", r);
}
```

对于一些复杂数据类型，可以通过struct并添加`#[repr(C)]`来保证和C的内存布局一致来组合复杂的数据类型

Step3: 编译运行

指定静态库的搜索路径

`rustc -L . call_math.rs`



## 文档和测试

* `///`开头的文档被视为是给它后面的那个元素做的说明
* `//!` 开头的文档被视为是给包含这块文档的元素做的说明

Rust文档里面的代码块，在使用cargo test命令时，也是会被当做测试用例执行的。
文档内部支持markdown格式

`#[doc(include = "external-doc.md")]` 直接引用外部文档

`#[cfg]`它主要是用于实现各种条件编译。比如`#[cfg(test)]`意思是，这部分代码只在test这个开关打开的时候才会被 编译。

```rust
#[cfg(any(unix, windows))]
#[cfg(all(unix, target_pointer_width = "32"))]
#[cfg(not(foo))]
#[cfg(any(not(unix), all(target_os="macos", target_arch = "powerpc")))]
```

自定义开关

```rust
[features]
# 默认开启的功能开关
default = []

# 定义一个新的功能开关,以及它所依赖的其他功能
# 我们定义的这个功能不依赖其他功能,默认没有开启
my_feature_name = []

// 然后在代码中进行引用
#[cfg(feature = "my_feature_name")]
  mod sub_module_name {
  }

这个功能是否开启，可以通过命令行进行传递
cargo build --features "my_feature_name"
```

`#[ignore]`标记测试用例，暂时忽略这个测试
`#[bench]`添加性能测试用例

## DST

* DST(胖指针)的设计，避免了数据类型作为参数传递时自动退化为裸指针类型，丢失长度信息的问题，保证了类型安全。
* enum中不能包含DST类型，struct中只有最后一个元素可以是DST，其他地方不行，如果包含有DST类型，那么这个结构体也就成了DST类型



## macro

Rust中的宏是在AST之后执行的，因此必须符合Rust语法，Rust针对语法扩展提供了下列几种语法形式。

1. `#[$arg]` 如 `#[derive(Clone)]`、`#[no_mangle]`
2. `#![$arg]`
3. `$name! $arg`
4. `$name! $arg0 $arg1`

而宏就属于上面的第三种。

item、block、stmt、 pat、expr、ty、

1. itent (is used for variable/function names)
2. path
3. tt ((token tree)
4. item
5. block
6. stmt (statement)
7. pat (pattern)
8. expr (is used for expressions)
9. ty (type)
10. literal (is used for literal constants)
11. vis(visibility qualifier)

```rust
macro_rules! MACRO_NAME {
}
```


hashmap宏的使用例子:

```rust
macro_rules! hashmap {
    ($key: expr => $val: expr) => {
        {
        let mut map = ::std::collections::HashMap::new();
        map.insert($key, $val);
        map
        }
    }
}
```

> 宏可以重载，也可以重复递归调用
> 宏的参数可以使用+号来表示重复1到多次、或者使用*表示重复0到多次


## 函数没有参数，但是却包含了生命周期参数，那么默认是'static

```rust
// `print_refs` takes two references to `i32` which have different
// lifetimes `'a` and `'b`. These two lifetimes must both be at
// least as long as the function `print_refs`.
fn print_refs<'a, 'b>(x: &'a i32, y: &'b i32) {
    println!("x is {} and y is {}", x, y);
}

// A function which takes no arguments, but has a lifetime parameter `'a`.
fn failed_borrow<'a>() {
    let _x = 12;

    // ERROR: `_x` does not live long enough
    // _x的生命周期显然没有'static长
    //let y: &'a i32 = &_x;
    // Attempting to use the lifetime `'a` as an explicit type annotation
    // inside the function will fail because the lifetime of `&_x` is shorter
    // than that of `y`. A short lifetime cannot be coerced into a longer one.
}

fn main() {
    // Create variables to be borrowed below.
    let (four, nine) = (4, 9);

    // Borrows (`&`) of both variables are passed into the function.
    print_refs(&four, &nine);
    // Any input which is borrowed must outlive the borrower.
    // In other words, the lifetime of `four` and `nine` must
    // be longer than that of `print_refs`.

    // 生命周期默认是'static
    failed_borrow();
    // `failed_borrow` contains no references to force `'a` to be
    // longer than the lifetime of the function, but `'a` is longer.
    // Because the lifetime is never constrained, it defaults to `'static`.
}

```

## 当数据被不可变借用的时候，那么将无法通过原来可变的变量进行数据的修改

```rust
fn main() {
    let mut _mutable_integer = 7i32;
    {
        // Borrow `_mutable_integer`
        // 做了不可变的借用
        let _large_integer = &_mutable_integer;
        // Error! `_mutable_integer` is frozen in this scope
        // 导致原来的的无法修改
        _mutable_integer = 50;
        // FIXME ^ Comment out this line
        // `_large_integer` goes out of scope
    }
    // Ok! `_mutable_integer` is not frozen in this scope
    _mutable_integer = 3;
}
```

## 当数据被move的时候可以改变其可变性

```rust
fn main() {
    let immutable_box = Box::new(5u32);

    println!("immutable_box contains {}", immutable_box);

    // Mutability error
    //*immutable_box = 4;

    // *Move* the box, changing the ownership (and mutability)
    let mut mutable_box = immutable_box;

    println!("mutable_box contains {}", mutable_box);

    // Modify the contents of the box
    *mutable_box = 4;

    println!("mutable_box now contains {}", mutable_box);
}
```



## error handle


## cargo

`Dev-dependencies` are not used when compiling a package for building, but are used for compiling tests, examples, and benchmarks.

[dev-dependencies]
tempdir = "0.3"



## attribute

This RFC introduces the #[non_exhaustive] attribute for enums and structs, which indicates that more variants/fields may be added to an enum/struct in the future.
Adding this hint to enums will force downstream crates to add a wildcard arm to match statements, ensuring that adding new variants is not a breaking change.
Adding this hint to structs or enum variants will prevent downstream crates from constructing or exhaustively matching, to ensure that adding new fields is not a breaking change.


## 技巧

```
pub fn write_record<I, T>(&mut self, record: I) -> csv::Result<()>
    // 具备迭代器的能力，并且满足AsRef<[u8]>，
    where I: IntoIterator<Item=T>, T: AsRef<[u8]>
{
    // implementation elided
}
```

* The AsRef<[u8]> bound is useful because types like `String`, `&str`, `Vec<u8>` and `&[u8]` all satisfy it.


## Link

* [Cfg Test and Cargo Test a Missing Information](https://freyskeyd.fr/cfg-test-and-cargo-test-a-missing-information/)
* [System V ABI read zone](https://os.phil-opp.com/red-zone/)
* [disbale SIMD](https://os.phil-opp.com/disable-simd/)
* [Too Many Linked Lists](https://rust-unofficial.github.io/too-many-lists/)
* [Interior mutability in Rust: what, why, how?](https://ricardomartins.cc/2016/06/08/interior-mutability)
* [& vs. ref in Rust patterns](http://xion.io/post/code/rust-patterns-ref.html)
* [Clear explanation of Rust’s module system](http://www.sheshbabu.com/posts/rust-module-system/)
* [Understanding Rust slices](https://codecrash.me/understanding-rust-slices)
* [Implement a bloom filter](https://onatm.dev/2020/08/10/let-s-implement-a-bloom-filter/)
* [Frustrated? It's not you, it's Rust](https://fasterthanli.me/articles/frustrated-its-not-you-its-rust)
* [Clear explanation of Rust’s module system](http://www.sheshbabu.com/posts/rust-module-system/?spm=ata.13261165.0.0.13f861b6Lw1Py4)
* https://depth-first.com/articles/2020/01/27/rust-ownership-by-example/

## TODO
1. https://github.com/stjepang/polling(done)
2. https://fasterthanli.me/articles/surviving-rust-async-interfaces
3. https://github.com/joshtriplett/async-pidfd
4. https://github.com/rust-lang/async-book(Done)
5. https://nick.groenen.me/posts/rust-error-handling/
6. https://stackoverflow.com/questions/63164973/why-does-rust-allow-calling-functions-via-null-pointers
7. https://blog.carlosgaldino.com/writing-a-file-system-from-scratch-in-rust.html
8. https://tokio.rs/tokio/tutorial
9. https://fasterthanli.me/articles/small-strings-in-rust
10. https://sokoban.iolivia.me/c01-00-intro.html
11. https://github.com/cloudhead/popol
12. http://www.sheshbabu.com/posts/rust-module-system/ (Done)
13. https://createlang.rs/
14. https://blog.thoughtram.io/string-vs-str-in-rust/
15. https://fasterthanli.me/articles/working-with-strings-in-rust
16. https://github.com/stjepang/smol
17. https://dev.to/anshulgoyal15/a-beginners-guide-to-grpc-with-rust-3c7o
18. https://cfsamson.github.io/books-futures-explained/introduction.html
19. http://sled.rs/errors
20. https://fasterthanli.me/articles/frustrated-its-not-you-its-rust (done)
21. https://medium.com/swlh/writing-a-modern-http-s-tunnel-in-rust-56e70d898700
22. https://lowlvl.org/lesson1.html
23. https://ferrous-systems.github.io/teaching-material/index.html
24. https://github.com/pretzelhammer/rust-blog/blob/master/posts/common-rust-lifetime-misconceptions.md(Done)
25. https://github.com/rust-lang/rfcs/blob/master/text/2025-nested-method-calls.md
26. https://github.com/pretzelhammer/rust-blog/blob/master/posts/sizedness-in-rust.md
27. https://github.com/rust-lang/rfcs/blob/master/text/2005-match-ergonomics.md(Done)
28. https://github.com/nox/rust-rfcs/blob/master/text/0738-variance.md
29. https://arzg.github.io/lang/
