---
hide:
  - toc        # Hide table of contents
---

# 常见的生命周期概念错误

## T 只包含 owned types

| | | | |
|-|-|-|-|
| **Type Variable** | `T` | `&T` | `&mut T` |
| **Examples** | `i32` | `&i32` | `&mut i32` |

实际上这是错误的，`T`包含了所有的类型、`&T`包含了所有引用类型，但是不包含owned types、`&mut T`包含了所有的可变引用类型。

| | | | |
|-|-|-|-|
| **Type Variable** | `T` | `&T` | `&mut T` |
| **Examples** | `i32`, `&i32`, `&mut i32`, `&&i32`, `&mut &mut i32`, ... | `&i32`, `&&i32`, `&&mut i32`, ... | `&mut i32`, `&mut &mut i32`, `&mut &i32`, ... |

可以看到`T` 和 `&T`、`&mut T` 之间是有重叠的，但是`&T`和`&mut T`是不重叠的。

```rust
trait Trait {}

impl<T> Trait for T {}

impl<T> Trait for &T {} // ❌

impl<T> Trait for &mut T {} // ❌
```

上面的程序是编译报错的，因为`T` 和 `&T`、`&mut T` 类型之间是有重叠的，不能对相同的类型有不同的实现，这显然是冲突的。

```rust
error[E0119]: conflicting implementations of trait `Trait` for type `&_`:
 --> src/lib.rs:5:1
  |
3 | impl<T> Trait for T {}
  | ------------------- first implementation here
4 |
5 | impl<T> Trait for &T {}
  | ^^^^^^^^^^^^^^^^^^^^ conflicting implementation for `&_`

error[E0119]: conflicting implementations of trait `Trait` for type `&mut _`:
 --> src/lib.rs:7:1
  |
3 | impl<T> Trait for T {}
  | ------------------- first implementation here
...
7 | impl<T> Trait for &mut T {}
  | ^^^^^^^^^^^^^^^^^^^^^^^^ conflicting implementation for `&mut _`
```

但是下面这样是OK的。

```rust
trait Trait {}

impl<T> Trait for &T {} // ✅

impl<T> Trait for &mut T {} // ✅
```

因为`&T`和`&mut T`是不重叠的。

!!! Note
    `T` 是 `&T`和`&mut T`的超集
    `&T`和`&mut T`是不冲突的


##  `T: 'static` 指的是T的生命周期直到程序退出为止

错误的概念:

* `T: 'static` 应该读作 `T` 有 `'static` 生命周期
* `&'static T` 和 `T: 'static`是相同的事情
* `T: 'static` 中 `T`必须是不可变
* `T: 'static` 中 `T`仅能创建在编译时

可能我们大部分人对于`'static`的认识可能是从下面这种代码开始的。

```rust
fn main() {
    let str_literal: &'static str = "str literal";
}
```

`"str literal"`是hardcode在二进制程序中的只读区域，其生命周期和程序的生命周期一致，除了`'static str`外，还有`static`变量，其生命周期也是和程序的生命周期一致。

```rust
// Note: This example is purely for illustrative purposes.
// Never use `static mut`. It's a footgun. There are
// safe patterns for global mutable singletons in Rust but
// those are outside the scope of this article.

static BYTES: [u8; 3] = [1, 2, 3];
static mut MUT_BYTES: [u8; 3] = [1, 2, 3];

fn main() {
   MUT_BYTES[0] = 99; // ❌ - mutating static is unsafe

    unsafe {
        MUT_BYTES[0] = 99;
        assert_eq!(99, MUT_BYTES[0]);
    }
}
```

`static`变量有一些特点:
1. 