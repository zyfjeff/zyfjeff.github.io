# Rust Variance


组合变型有两种数学运算：Transform 和最大下确界（greatest lower bound, GLB ）。Transform 用于类型组合，而 GLB 用于所有的聚合体：结构体、元组、枚举以及联合体。让我们分别用 0、+、和 - 来表示不变，协变和逆变。然后 Transform（X）和 GLB（^）可以用下面两张表来表示：

|   |                 |     'a    |         T         |     U     |
|---|-----------------|:---------:|:-----------------:|:---------:|
| * | `&'a T `        | covariant | covariant         |           |
| * | `&'a mut T`     | covariant | invariant         |           |
| * | `Box<T>`        |           | covariant         |           |
|   | `Vec<T>`        |           | covariant         |           |
| * | `UnsafeCell<T>` |           | invariant         |           |
|   | `Cell<T>`       |           | invariant         |           |
| * | `fn(T) -> U`    |           | **contra**variant | covariant |
|   | `*const T`      |           | covariant         |           |
|   | `*mut T`        |           | invariant         |           |



```rust
use std::fmt;
fn announce<'a, T>(value: &'a T) where T: fmt::Display {
    println!("Behold! {}!", value);
}
fn main() {
    {                                       // 'x
        let num = 42;
        {                                   // 'y
            let num_ref = &num;
            {                               // 'z
                announce(num_ref);
            }
        }
    }
}
```

```rust
fn evil_feeder<T>(input: &mut T, val: T) {
    *input = val;
}

fn main() {
    let mut mr_snuggles: &'static str = "meow! :3";  // mr. snuggles forever!!
    {
        let spike = String::from("bark! >:V");
        let spike_str: &str = &spike;                // Only lives for the block
        evil_feeder(&mut mr_snuggles, spike_str);    // EVIL!
    }
    println!("{}", mr_snuggles);                     // Use after free?
}
```


```rust
struct Owner<'a:'c, 'b:'c, 'c> {
    pub dog: &'a &'c str,
    pub cat: &'b mut &'c str,
}

fn main() {
    let mut mr_snuggles: &'static str = "meow! :3";
    let spike = String::from("bark! >:V");
    let spike_str: &str = &spike;
    let alice = Owner { dog: &spike_str, cat: &mut mr_snuggles };
}
```


```rust
use std::cell::Cell;

struct MyType<'a, 'b, A: 'a, B: 'b, C, D, E, F, G, H, In, Out, Mixed> {
    a: &'a A,     // covariant over 'a and A
    b: &'b mut B, // covariant over 'b and invariant over B

    c: *const C,  // covariant over C
    d: *mut D,    // invariant over D

    e: E,         // covariant over E
    f: Vec<F>,    // covariant over F
    g: Cell<G>,   // invariant over G

    h1: H,        // would also be variant over H except...
    h2: Cell<H>,  // invariant over H, because invariance wins all conflicts

    i: fn(In) -> Out,       // contravariant over In, covariant over Out

    k1: fn(Mixed) -> usize, // would be contravariant over Mixed except..
    k2: Mixed,              // invariant over Mixed, because invariance wins all conflicts
}
```

https://www.cnblogs.com/praying/p/14243566.html
https://zhuanlan.zhihu.com/p/41814387
https://learnku.com/docs/nomicon/2018/38-subtypes-and-variability/4719
https://github.com/rust-lang/reference/blob/master/src/subtyping.md