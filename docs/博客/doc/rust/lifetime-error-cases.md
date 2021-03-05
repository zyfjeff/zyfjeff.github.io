---
hide:
  - toc        # Hide table of contents
---
# 常见的生命周期错误


* 示例1

```rust
struct S<'a> {
    x: &'a i32,
    y: &'a i32
}

fn main() {
    let x = 10;
    let r;
    {
        let y = 20;
        {
            let s = S { x: &x, y: &y };
            r = s.x;
        }
    }
    println!("{} {}", x, r);
}
```


**错误分析**

```rust
   |
12 |             let s = S { x: &x, y: &y };
   |                                   ^^ borrowed value does not live long enough
...
15 |     }
   |     - `y` dropped here while still borrowed
16 |     println!("{} {}", x, r);
   |                          - borrow later used here
```

实际上，上面这段代码错误的原因是`S<'a>`中的两个字段的生命周期都是`'a`，这导致`let s = S { x: &x, y: &y };`和`r = s.x`两个表达式存在歧义，前者要求x的生命周期和y一致，这个可以通过协变达成。但是后者要求`s.x`的生命周期要大于y。 
这就尴尬了，你是没办法找出这样的一个生命周期大于y并且等于y。所以上面报错了。

**解决方案**

只需要将`S`的两个字段用不同的生命周期标注即可。这样就可以满足上面代码对于生命周期的要求。

```rust

struct S<'a, 'b> {
    x: &'a i32,
    y: &'b i32
}
```


* 示例2

```rust
  struct ByteIter<'a> {
    remainder: &'a [u8]
  }

  impl<'a> ByteIter<'a> {
      fn next(&mut self) -> Option<&u8> {
          if self.remainder.is_empty() {
              None
          } else {
              let byte = &self.remainder[0];
              self.remainder = &self.remainder[1..];
              Some(byte)
          }
      }
  }

  fn main() {
      let mut bytes = ByteIter { remainder: b"1" };
      let byte_1 = bytes.next();
      let byte_2 = bytes.next();
      if byte_1 == byte_2 {
          // do something
      }
  }
```

**错误分析**

```rust
error[E0499]: cannot borrow `bytes` as mutable more than once at a time
  --> src/main.rs:20:18
   |
19 |     let byte_1 = bytes.next();
   |                  ----- first mutable borrow occurs here
20 |     let byte_2 = bytes.next();
   |                  ^^^^^ second mutable borrow occurs here
21 |     if byte_1 == byte_2 {
   |        ------ first borrow later used here
```

错误的原因是因为rust默认给next添加了生命周期注解，展开后如下:

```rust
  fn next(&'b mut self) -> Option<&'b u8>
```

这导致编译器认为返回的`Option<&'b u8>`是对`&b mut self`的引用，而这里是一个典型的`&mut`降级为`&`的场景。当第二次再调用next方法的时候会再次创建`&mut self`引用。
我们都知道这是没办法创建的，因为`&mut`和`&`不能同时出现。


**解决方案**

只需要将`Option<&u8>`改成`Option<&'a u8>`即可，让返回值指向remainder，那么编译器就不会认为是对`&mut self`的引用，也自然不会有问题了。

* 示例3

```rust
#[derive(Debug)]
struct NumRef<'a>(&'a i32);

impl<'a> NumRef<'a> {
    // 我定义的泛型结构体以 'a 为参数，这意味着我也需要给方法的参数
    // 标注为 'a 生命周期，对吗？（答案：错）
    fn some_method(&'a mut self) {}
}

fn main() {
    let mut num_ref = NumRef(&5);
    num_ref.some_method(); // 可变借用 num_ref 直至其生命周期结束
    num_ref.some_method(); // 编译错误
    println!("{:?}", num_ref); // 同样编译错误
}

```

**错误分析**

上面代码错误的原因是因为`some_method`方法被显示设置了生命注解`'a`导致在调用some_method的时候误认为`&'a mut self`引用了`num_ref`，而`num_ref`的生命周期是直到程序结束，因此
some_method的调用导致`&mut self`一直被持有直到程序结束，因此也无法再调用。

```rust
error[E0499]: cannot borrow `num_ref` as mutable more than once at a time
  --> src/main.rs:13:5
   |
12 |     num_ref.some_method(); // 可变借用 num_ref 直至其生命周期结束
   |     ------- first mutable borrow occurs here
13 |     num_ref.some_method(); // 编译错误
   |     ^^^^^^^
   |     |
   |     second mutable borrow occurs here
   |     first borrow later used here
```

**解决方案**

把`&mut self`和`num_ref`的关系解，不要用相同的生命周期，使用编译器默认给我`some_method`添加的生命周期即可。 

```rust
impl<'a> NumRef<'a> {
  fn some_method(&mut self) {}
}
```

* 示例4

```rust
use std::fmt::Display;

fn dynamic_thread_print(t: Box<dyn Display + Send>) {
    std::thread::spawn(move || {
        println!("{}", t);
    }).join();
}

fn static_thread_print<T: Display + Send>(t: T) {
    std::thread::spawn(move || {
        println!("{}", t);
    }).join();
}
```

**错误分析**

```rust
error[E0310]: the parameter type `T` may not live long enough
  --> src/main.rs:10:5
   |
9  | fn static_thread_print<T: Display + Send>(t: T) {
   |                        -- help: consider adding an explicit lifetime bound...: `T: 'static +`
10 |     std::thread::spawn(move || {
   |     ^^^^^^^^^^^^^^^^^^ ...so that the type `[closure@src/main.rs:10:24: 12:6]` will meet its required lifetime bounds
```

这是因为`std::thread::spawn`要求其内部捕获的类型需要满足`'static`约束，因此会编译出错，要求给T添加`'static`约束，但是为什么`dynamic_thread_print`函数中没有要求t有`'static`的生命周期约束呢？
这是因为编译器给我们默认添加了。对于所有的`trait`对象编译器有着一套规则来给其添加生命周期约束，下面是具体的规则。


```rust
use std::cell::Ref;

trait Trait {}

// 展开前
type T1 = Box<dyn Trait>;
// 展开后，Box<T> 没有对 T 的生命周期约束，所以推导为 'static
type T2 = Box<dyn Trait + 'static>;

// 展开前
impl dyn Trait {}
// 展开后
impl dyn Trait + 'static {}

// 展开前
type T3<'a> = &'a dyn Trait;
// 展开后，&'a T 要求 T: 'a, 所以推导为 'a
type T4<'a> = &'a (dyn Trait + 'a);

// 展开前
type T5<'a> = Ref<'a, dyn Trait>;
// 展开后，Ref<'a, T> 要求 T: 'a, 所以推导为 'a
type T6<'a> = Ref<'a, dyn Trait + 'a>;

trait GenericTrait<'a>: 'a {}

// 展开前
type T7<'a> = Box<dyn GenericTrait<'a>>;
// 展开后
type T8<'a> = Box<dyn GenericTrait<'a> + 'a>;

// 展开前
impl<'a> dyn GenericTrait<'a> {}
// 展开后
impl<'a> dyn GenericTrait<'a> + 'a {}
```

**解决方案**

按照编译器给的提示给`T`添加生命周期注解即可。


* 示例5

```rust
struct Has<'lifetime> {
    lifetime: &'lifetime str,
}

fn main() {
    let long = String::from("long");
    let mut has = Has { lifetime: &long };
    assert_eq!(has.lifetime, "long");

    // 这个代码块逻辑上永远不会被执行
    if false {
        let short = String::from("short");
        // “转换到” 短的生命周期
        has.lifetime = &short;
        assert_eq!(has.lifetime, "short");

        // “转换回” 长的生命周期（实际是并不是）
        has.lifetime = &long;
        assert_eq!(has.lifetime, "long");
        // `short` 变量在这里 drop
    }

    // 还是编译失败， `short` 在 drop 后仍旧处于 “借用” 状态
    assert_eq!(has.lifetime, "long");
}
```

**错误分析**

```rust
error[E0597]: `short` does not live long enough
  --> src/main.rs:14:24
   |
14 |         has.lifetime = &short;
   |                        ^^^^^^ borrowed value does not live long enough
...
21 |     }
   |     - `short` dropped here while still borrowed
...
24 |     assert_eq!(has.lifetime, "long");
   |     --------------------------------- borrow later used here
```

错误的原因是因为Rust中的生命周期是在编译时一旦确定就无法更改的，虽然我们可以让短的生命周期指向长的，但是这一切都是在编译期确定的，Rust编译器会分析代码中任何的执行流，并选择一个最短的生命周期。
例如下面这段代码，Rust在编译时决定了`lifetime`的生命周期和`short`一样长。

```rust
struct Has<'lifetime> {
    lifetime: &'lifetime str,
}

fn main() {
    let short = String::from("short");
    let mut has = Has { lifetime: &short };
    assert_eq!(has.lifetime, "short");
    has.lifetime = &"long";
    assert_eq!(has.lifetime, "long");
}
```


## Reference

* [2094-nll](https://github.com/rust-lang/rfcs/blob/master/text/2094-nll.md)
* [common-rust-lifetime-misconceptions](https://github.com/pretzelhammer/rust-blog/blob/master/posts/translations/zh-hans/common-rust-lifetime-misconceptions.md)
* [hrtb](https://doc.rust-lang.org/nomicon/hrtb.html)