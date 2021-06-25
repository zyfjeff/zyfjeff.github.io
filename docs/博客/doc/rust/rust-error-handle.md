
# Rust错误处理指南

作为一名`C++er`初学Rust的时候被它的错误处理弄的晕头转向，错误的处理方式很多，有针对的Option类型的，还有针对Result类型的，还可以自定义错误类型，
以及使用诸如failure这样的三方库来进行错误处理等，为此我试图梳理本文，将常见的错误处理方式进行总结。

## `panic!`

第一种错误的处理方式，最为简单，也是最为粗暴的，直接结束程序，典型的几个使用场景如下:

* Out-of-bounds array access
* Integer division by zero
* Calling .unwrap() on an Option that happens to be None
* Assertion failure

下面是因为除0导致`panic`的一个例子:

```rust
fn pirate_share(total: u64, crew_size: usize) -> u64 {
    let half = total / 2;
    half / crew_size as u64
}

fn main() {
    pirate_share(100, 0);
}
```

一不小心传入了0后会导致除以0，触发`panic`，`panic`的调用会导致栈解旋(unwinding)，最后使得进程退出，这个过程其实类似C++的异常没有捕获的效果。
但是和C++异常不同的是，`panic`发生的线程如果不是主线程是不会导致程序退出的。而C++异常会。下面是一个小的例子：

> 栈解旋会对调用对象的自定义的drop方法进行资源清理，以及释放内存、关闭文件等

```rust
fn pirate_share(total: u64, crew_size: usize) -> u64 {
    let half = total / 2;
    half / crew_size as u64
}

fn main() {
    let handle = std::thread::spawn(||{
        pirate_share(100, 0);
    });

    handle.join().unwrap_or(());
    println!("exit success");
}
```

将`panic!`的代码放入一个单独的线程中运行，线程`panic!`并没有影响主线程中这条语句的执行`println!("exit success");`。

如果在`unwinding`的时候，调用对象的自定义drop方法时再次触发`panic`这会导致堆栈解旋结束，直接退出程序，例如下面这个例子:


```rust

struct Test {
    is_panic: bool,
    count: u64,
}

impl Drop for Test {
    fn drop(&mut self) {
        if self.is_panic {
            panic!("next panic");
        }
        println!("Dropping! {}", self.count);
    }
}

fn pirate_share(total: u64, crew_size: usize) -> u64 {
    let half = total / 2;
    half / crew_size as u64
}

fn main() {
    let x0 = Test{
        count: 0,
        is_panic: false,
    };
    let x1 = Test{
        count: 1,
        is_panic: false,  // 改成true后会导致没有任何输出
    };
    pirate_share(100, 0);
}
```

上面的代码运行后输出:

```bash
Dropping! 1
Dropping! 0
```

这是因为`panic!`触发了`unwinding`导致自定义的drop方法的调用，按照逆序(先构造的对象后调用)进行了输出，所以结果就是上面这个样子了。
如果将`x1`对象的`is_panic`属性设置为true，会触发第二次panic，这个时候会直接退出整个进程，不做任何清理动作，所以不会有任何输出。
`panic!`的调用会触发`unwinding`，这只是默认行为，并不代表一定要做unwinding，也可以选择直接退出进程，不做任何清理动作，带来的好处就是可以减少代码的大小。
可以通过给rustc添加`-C panic=abort`选项来开启， 如果是cargo的话，可以通过profile来设置

```rust
[profile.dev]
panic = "abort"

[profile.release]
panic = "abort"
```

## `Option`

Rust中错误处理可以使用Option来实现，如果成功就返回`Some(value)`，如果失败就返回`None`

1. `match`

```rust
fn main() {
    let file_name = "footbar.rs";
    match file_name.find('.') {
        None => println!("No file extension found."),
        Some(i) => println!("File extension: {}", &file_name[i+1..]),
    }
}
```

find方法返回的就是`Option<usize>`，如果成功找到就返回对应的位置，否则就返回`None`，类比于其他语言可能就是通过返回一个`-1`来表示没有找到。
这种通过match模式匹配的方式显得代码非常臃肿，我们希望可以通过一些简单的方法来实现错误处理。

2. `unwrap`

其实现大致如下，通过match进行匹配，如果是`None`就`panic`

```rust
enum Option<T> {
    None,
    Some(T),
}

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

3. `map`

如果执行成功，那么接下来我们可能会对其返回值进行进一步的处理，这个时候可以通过map系列方法进行链式处理。

map的实现类似下面代码，当成功返回的时候，可以将返回值传递到一个callback中进行回调，最后将callback的返回值封装成Option类型。
从而实现`Option<T>` 到 `Option<A>`的类型转换。

```rust
fn map<F, T, A>(option: Option<T>, f: F) -> Option<A> where F: FnOnce(T) -> A {
    match option {
        None => None,
        Some(value) => Some(f(value)),
    }
}
```

针对上面的例子，通过map对返回值进行了二次处理。

```rust
fn main() {
    let file_name = "footbar.rs";
    match file_name.find('.').map(|value| {
        // skip .
        return value + 1;
    }) {
        Some(value) => println!("File extension: {}", &file_name[value..]),
        None =>  println!("No file extension found."),
    }
}
```

4. `map_or`

需要提供一个默认值，当Option是None的时候，就会返回这个默认值，此外还需要提供一个callback，用于当Option是Some的时候进行回调处理

```rust
    pub fn map_or<U, F: FnOnce(T) -> U>(self, default: U, f: F) -> U {
        match self {
            Some(t) => f(t), 
            None => default,
        }
    }
```

 下面是一个使用的例子，通过`map_or`提供默认值，整个map系列都是对结果的二次处理，所以可以返回一个新的类型。

```rust
fn main() {
    let file_name = "footbar.rs";
    let extensions = file_name.find('.').map_or("exe".to_owned(), |value|{
        return String::from(&file_name[value + 1..]);
    });
    println!("extensions: {}", extensions);
}
```

5. `map_or_else`

和`map_or`类似，不同的是，`map_or`是直接提供默认值，而`map_or_else`则是通过callback的方式来提供默认值。

```rust
    pub fn map_or_else<U, D: FnOnce() -> U, F: FnOnce(T) -> U>(self, default: D, f: F) -> U {
        match self {
            Some(t) => f(t),
            None => default(),
        }
    }
```


6. `unwrap_or`

`unwrap`在等于`None`的时候通过`panic`来结束程序，而`unwrap_or`则是通过提供默认值的方式来处理。

```rust
fn main() {
    let file_name = "footbar.rs";
    let pos = file_name.find('.').unwrap_or(0);
    println!("pos = {}, extensions: {}", pos, &file_name[pos + 1..]);
}
```

7. `unwrap_or_else`

`unwrap_or_else`和`unwrap_or`都是针对`None`来处理的，但是前者是通过callback的方式来提供默认值，而后者是直接提供默认值。

```rust
fn main() {
    let file_name = "footbar.rs";
    let pos = file_name.find('.').unwrap_or_else(||{
        0
    });
    println!("pos = {}, extensions: {}", pos, &file_name[pos + 1..]);
}
```

8. `and_then`

相比与map，and_then区别并不大，前者是返回value，然后被包装成一个Option，后者是直接返回Option，除此之外并没有其他区别。

```rust
fn and_then<F, T, A>(option: Option<T>, f: F) -> Option<A>
        where F: FnOnce(T) -> Option<A> {
    match option {
        None => None,
        Some(value) => f(value),
    }
}
```

## `Result`

Rust中没有异常，错误都是通过返回值来表示，而`Option`只能表示是否发生了错误，如果发生了错误，错误的内容是什么，并没有办法传递出来，而`Result`可以，其定义如下:

```rust
enum Result<T, E> {
    Ok(T),
    Err(E),
}
```

下面是一个用`Result`来传递错误内容的例子:

```rust
fn get_weather(location: LatLng) -> Result<WeatherReport, io::Error>
```

针对返回的Result类型有几种处理方式，第一种最为大家熟悉的就是`match`进行模式匹配。

1. `match`

```rust
match get_weather(hometown) {
    Ok(report) => {
        display_weather(hometown, &report);
    }
    Err(err) => {
        println!("error querying the weather: {}", err);
        schedule_weather_retry();
    }
}
```

2. `is_error` and `is_ok`

```rust

use std::fs::File;

fn main() {
    let f = File::open("/tmp/test");
    if f.is_err() {
        println!("open file failure");
    }
}
```

通过`is_error`和`is_ok`可以知道是否成功还是失败，要想拿到成功返回的值或者失败返回的错误需要通过`ok()`和`err()`方法，这两个方法返回的都是Option，所以还需要再解一次Option

```rust
use std::fs::File;

fn main() {
    let f = File::open("/tmp/test");
    if f.is_err() {
        println!("open file failure {}", f.err().unwrap());
    } else {
        println!("open file success {:?}", f.ok().unwrap());
    }
}
```

3. `unwrap_or(fallback)`

仅当成功时候直接返回成功的值，如果是错误就返回提供的默认值fallback

```rust

fn test_error(is_ok: bool) -> Result<String, std::io::Error> {
    if is_ok {
        return Ok("test".to_owned());
    }
    return Err(std::io::Error::from(std::io::ErrorKind::Other))
}

fn main() {
    let default_result = test_error(false).unwrap_or("default".to_owned());
    assert_eq!(default_result, "default".to_owned());
    let return_result = test_error(true).unwrap_or("default".to_owned());
    assert_eq!(return_result, "test".to_owned())
}
```

4. `unwrap_or_else(fallback_fn)`

和`unrwap_or`类似，只不过是通过callback的方式来提供默认值，而不是直接提供默认值


```rust
fn test_error(is_ok: bool) -> Result<String, std::io::Error> {
    if is_ok {
        return Ok("test".to_owned());
    }
    return Err(std::io::Error::from(std::io::ErrorKind::Other))
}

fn main() {
    let default_result = test_error(false).unwrap_or_else(|_err|{
        "default".to_owned()
    });
    assert_eq!(default_result, "default".to_owned());
    let return_result = test_error(true).unwrap_or_else(|_err|{
        "default".to_owned()
    });
    assert_eq!(return_result, "test".to_owned())
}
```

5. `ok_or`

相比于`Option`，`Result`提供了错误类型，通过`ok_or`可以将一个`Option`转换为`Result`

```rust
  pub fn ok_or<E>(self, err: E) -> Result<T, E> {
      match self {
          Some(v) => Ok(v),
          None => Err(err),
      }
  }
```

```rust

fn test_option(is_none: bool) -> Option<String> {
    if is_none {
        None
    } else {
        "test".to_owned()
    }
}

fn main() {
  // 将option<String> 转换为 Result<String, std::io::ErrorKind>
  let result = test_option(false).ok_or(std::io::ErrorKind::NotFound);
}
```

6. `map` and `map_err`

当返回成功的时候，通过map来处理返回的值，而当返回错误的时候，通过`map_err`来处理错误的值。

```rust
use std::fs;
use std::io::Write;

fn main() {
    let file = fs::File::open("/tmp/test").as_mut().map(|file| {
        file.write(&[1, 2, 4]);
        // 可以返回另外一种类型，最后会被自动添加Ok(file)
        return file;
    }).map_err(|err|{
        println!("Open error {}", err);
        //  可以返回另外一种错误类型，同样会自动添加Err(std::io::ErrorKind::NotFound)
        return std::io::ErrorKind::NotFound;
    });
}
```

7. `and_then`

将`Result`成功返回的值传递给callback，这个callback可以返回另外一种类型的成功值，进而实现类型转换，和map不同的是，需要自己包装上`Ok`

```rust
fn main() {
    let number_string = "125".to_owned();
    // 将Result<i32, ParseIntError> 转换为Result<String, ParseIntError>
    let number = number_string.parse::<i32>().and_then(|value| {
        Ok("string_type".to_owned())
    });
}
```


## 组合处理`Option`和`Result`

下面是一个代码中需要同时处理`Option`和`Result`两种返回类型，通过最简单的unwrap来处理。

```rust
use std::env;

fn main() {
    let mut argv = env::args();
    // 返回的是Option类型
    let arg: String = argv.nth(1).unwrap(); // error 1
    // 返回的是Result类型
    let n: i32 = arg.parse().unwrap(); // error 2
    println!("{}", 2 * n);
}
```

如果将`argv.nth`和`arg.parse`放到一个函数中，如何统一返回值? 我们不能再简单的通过unwrap结束程序的方式来处理了。Option和Result相互转换才是最佳解决方案。

```rust
use std::env;

fn double_arg(mut argv: env::Args) -> Result<i32, String> {
    argv.nth(1)
        // 通过ok_or转换为Result
        .ok_or("Please give at least one argument".to_owned())
        // 通过and_then将Result<String, String>，转换为<i32, String>
        //
        .and_then(|arg| arg.parse::<i32>().map_err(|err| err.to_string()))
}

fn main() {
    match double_arg(env::args()) {
        Ok(n) => println!("{}", n),
        Err(err) => println!("Error: {}", err),
    }
}
```

接下来我们看一个更复杂的例子，这个例子可能会返回多种错误，我们先使用最简单的unwrap版本

```rust
use std::fs::File;
use std::io::Read;
use std::path::Path;

fn file_double<P: AsRef<Path>>(file_path: P) -> i32 {
    let mut file = File::open(file_path).unwrap(); // error 1
    let mut contents = String::new();
    file.read_to_string(&mut contents).unwrap(); // error 2
    let n: i32 = contents.trim().parse().unwrap(); // error 3
    2 * n
}

fn main() {
    let doubled = file_double("foobar");
    println!("{}", doubled);
}
```

接下来我们开始改造，将错误通过`Result`返回。

```rust
use std::fs::File;
use std::io::Read;
use std::path::Path;

fn file_double<P: AsRef<Path>>(file_path: P) -> Result<i32, String> {
    File::open(file_path)
          // 将Result<File, Error> 转换为Result<File, String>
         .map_err(|err| err.to_string())
         // 接着将Result<File, String> 转换为Result<String, String>
         .and_then(|mut file| {
              let mut contents = String::new();
              file.read_to_string(&mut contents)
                  .map_err(|err| err.to_string())
                  .map(|_| contents)
         })
         // 将Result<String, String> 转换为 <i32, String>
         .and_then(|contents| {
              contents.trim().parse::<i32>()
                      .map_err(|err| err.to_string())
         })
         // 对最后的结果进行了处理
         .map(|n| 2 * n)
}

fn main() {
    match file_double("foobar") {
        Ok(n) => println!("{}", n),
        Err(err) => println!("Error: {}", err),
    }
}
```

通过`Option`和`Result`相互转换，可以将几种错误类型转换为统一的一种，但是如果错误类型有很多种，例如三种，通过组合`map`、`map_err`、`and_then`会使得整个
链式调用越来越长。而且很多人并不是熟悉这种通过组合器来处理错误的方式。如果将这些错误分开来处理，遇到错误就return，会使得整个代码更清晰，也更容易被其他人接受。
例如通过early return的方式改写上面的代码，如下:

```rust
use std::fs::File;
use std::io::Read;
use std::path::Path;

fn file_double<P: AsRef<Path>>(file_path: P) -> Result<i32, String> {
    let mut file = match File::open(file_path) {
        Ok(file) => file,
        Err(err) => return Err(err.to_string()),
    };
    let mut contents = String::new();
    if let Err(err) = file.read_to_string(&mut contents) {
        return Err(err.to_string());
    }
    let n: i32 = match contents.trim().parse() {
        Ok(n) => n,
        Err(err) => return Err(err.to_string()),
    };
    Ok(2 * n)
}

fn main() {
    match file_double("foobar") {
        Ok(n) => println!("{}", n),
        Err(err) => println!("Error: {}", err),
    }
}
```

改成early return的方式后，代码反而更啰嗦了，充斥了大量的match模式匹配，这不是我们倡导的编程实践。我们从一开始就试图通过组合器来消除match匹配。
通过`?`可以使得early return的这种方式更清晰，避免大量的match模式匹配。

```rust
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
```

到此为止我们的代码，看起来已经比较清晰了，但是仍然有一些`map_err`的调用，用来进行错误类型的转换，统一转换成`String`。但是这种方式是有缺点的。使用`String`表示错误会使得代码混乱，
所有的错误都是`String`类型，如果是针对最终的用户，那么返回`String`类型是合理的，可以告诉用户发生了什么错误，但是如果是内部的调用就不合理了，没办法根据`String`的内容进行错误类型区分。
如果我们能将遇到的错误类型自动转换为一种统一的类型，并且可以区分是何种错误， 那么就可以省略掉这些`map_err`的调用了。幸运的是，这有很多种方式来实现。

## 自定义错误类型

```rust
use std::io;
use std::num;

// We derive `Debug` because all types should probably derive `Debug`.
// This gives us a reasonable human readable description of `CliError` values.
#[derive(Debug)]
enum CliError {
    Io(io::Error),
    Parse(num::ParseIntError),
}
```

通过自定义错误类型可以解决`String`类型带来的缺点，下面使用`CliError`来替换`String`

```rust
use std::fs::File;
use std::io::Read;
use std::path::Path;

fn file_double<P: AsRef<Path>>(file_path: P) -> Result<i32, CliError> {
    let mut file = File::open(file_path).map_err(CliError::Io)?;
    let mut contents = String::new();
    file.read_to_string(&mut contents).map_err(CliError::Io)?;
    let n: i32 = contents.trim().parse().map_err(CliError::Parse)?;
    Ok(2 * n)
}

fn main() {
    match file_double("foobar") {
        Ok(n) => println!("{}", n),
        Err(err) => println!("Error: {:?}", err),
    }
}
```

通过自定义错误类型，就可以知道发生的错误到底是哪种类型的，但还是没有省略掉对`map_err`的调用，要解决这个问题就需要引入`From traits`，

```rust
trait From<T> {
    fn from(T) -> Self;
}

impl From<io::Error> for CliError {
    fn from(err: io::Error) -> CliError {
        CliError::Io(err)
    }
}

impl From<num::ParseIntError> for CliError {
    fn from(err: num::ParseIntError) -> CliError {
        CliError::Parse(err)
    }
}
```

通过引入`From traits`，我们就可以不用使用map_err了，可以通过自动调用`From traits`，将一些标准错误转换为自定义错误了，转换后的代码如下:

```rust
fn file_double<P: AsRef<Path>>(file_path: P) -> Result<i32, CliError> {
    let mut file = File::open(file_path)?;
    let mut contents = String::new();
    file.read_to_string(&mut contents)?;
    let n: i32 = contents.trim().parse()?;
    Ok(2 * n)
}
```

但是这种模式存在一个问题，就是没办法很好的扩展，我们需要给`file_double`函数种返回的所有可能的错误都添加转换到自定义错误的`From traits`。 我们是否有更好的办法来解决这个问题呢?
Rust标准库种实现了下面这组`From traits`

```rust
// 所有的Error都可以自动通过From traits自动转换为Box<Error>
impl<'a, E: Error + 'a> From<E> for Box<Error + 'a>
```

因为所有的满足Error特征的类型，都可以转换为`Box<Error>`，因此我们其实只要将返回类型改成`Box<Error>`即可，这样无论`file_double`中会返回何种错误类型都可以自动转换为`Box<dyn Error>`

```rust
use std::error::Error;
use std::fs::File;
use std::io::Read;
use std::path::Path;

fn file_double<P: AsRef<Path>>(file_path: P) -> Result<i32, Box<dyn Error>> {
    let mut file = File::open(file_path)?;
    let mut contents = String::new();
    file.read_to_string(&mut contents)?;
    let n = contents.trim().parse::<i32>()?;
    Ok(2 * n)
}

fn main() {
    let _res = file_double("/tmp/test");
}
```

那自定义的错误是否也可以转换为`Error`呢?

```rust
#[derive(Debug)]
enum CliError {
    Io(io::Error),
    Parse(num::ParseIntError),
}

fn file_double<P: AsRef<Path>>(file_path: P) -> Result<i32, Box<dyn std::error::Error>> {
    let mut contents = String::new();
    let n: i32 = contents.trim().parse().map_err(CliError::Parse)?;
    Ok(2 * n)
}

fn main() {
    match file_double("foobar") {
        Ok(n) => println!("{}", n),
        Err(err) => println!("Error: {:?}", err),
    }
}

// 报错如下:
error[E0277]: the trait bound `CliError: std::error::Error` is not satisfied
  --> src/main.rs:17:66
   |
17 |     let n: i32 = contents.trim().parse().map_err(CliError::Parse)?;
   |                                                                  ^ the trait `std::error::Error` is not implemented for `CliError`
   |
   = note: required because of the requirements on the impl of `std::convert::From<CliError>` for `std::boxed::Box<dyn std::error::Error>`
   = note: required by `std::convert::From::from`
```

自定义的错误看来并没有实现`std::error::Error`，接下来为我们自定义的错误实现`std::error::Error`特征。

```rust
impl fmt::Display for CliError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            // Both underlying errors already impl `Display`, so we defer to
            // their implementations.
            CliError::Io(ref err) => write!(f, "IO error: {}", err),
            CliError::Parse(ref err) => write!(f, "Parse error: {}", err),
        }
    }
}

impl error::Error for CliError {
    fn description(&self) -> &str {
        // Both underlying errors already impl `Error`, so we defer to their
        // implementations.
        match *self {
            CliError::Io(ref err) => err.description(),
            // Normally we can just write `err.description()`, but the error
            // type has a concrete method called `description`, which conflicts
            // with the trait method. For now, we must explicitly call
            // `description` through the `Error` trait.
            CliError::Parse(ref err) => error::Error::description(err),
        }
    }

    fn cause(&self) -> Option<&error::Error> {
        match *self {
            // N.B. Both of these implicitly cast `err` from their concrete
            // types (either `&io::Error` or `&num::ParseIntError`)
            // to a trait object `&Error`. This works because both error types
            // implement `Error`.
            CliError::Io(ref err) => Some(err),
            CliError::Parse(ref err) => Some(err),
        }
    }
}
```

到此为止，那么这个自定义的错误就可以自动转换为`std::error::Error`了。相比于通过`From traits`来处理错误的方法来说，这个方法可扩展性更佳，
但缺点就是`Box<dyn std::error::Error>`无法知道其具体的错误类型。需要自己通过`downcast_ref`进行类型转换，除此之外就只能通过`description`和`cause`方法获取到错误描述和导致错误的原因。

```rust
use std::error::Error;
use std::fmt::{self, Display, Formatter};

#[derive(Debug)]
struct MyError { }

impl Error for MyError {
    fn description(&self) -> &str {
        "description"
    }
}

impl Display for MyError {
    fn fmt(&self, formatter: &mut Formatter) -> Result<(), fmt::Error> {
        write!(formatter, "description")
    }
}

fn main() {
    let error: Box<Error> = Box::new(MyError {});
    let downcasted = error.downcast_ref::<MyError>();
    println!("{:?}", downcasted); // OK: prints Some(MyError).
}
```

使用起来还是略显麻烦，需要做一次转型，还需要判断是否转型成功等，那我们是否有更好的办法呢? Rust生态中，有很多用来处理错误的`crate`，接下来让我们看看这些`crates`是如何处理好这些事情的。


## 错误处理crates

`error-chain`

## 总结


## 参考文献

* [error-handling-survey](https://blog.yoshuawuyts.com/error-handling-survey/)
* [rust-error-handling](https://blog.burntsushi.net/rust-error-handling/)