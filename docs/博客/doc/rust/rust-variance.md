## Variance

"型变"或者称之为"变型"，在许多编程语言中都存在，型变指的是两个通过"类型构造器"构造的复杂类型和其"组件类型"之间的相关性，这句话看起来很复杂，让我们一点点来解析这句话。
首先来看下什么是类型构造器，例如C++中的`std::vector<T>`，这就是一个类型构造器，又比如Rust中的`Box<T>`、`Vec<T>`等(可以简单的把类型构造器理解成泛型，除了泛型外还有其他的类型构造器)。
通过这些构造构造器组合一个另外的"组件类型"，形成了一个新的类型。而型变指的就是通过类型构造器创建的复杂类型和其对应的组件类型之间的类型相关性。比如T是U的子类型，T可以转换为U，
那么`std::vector<T>`和`std::vector<U>`是什么关系呢? 如果构造的复杂类型和其组件类型的关系一致那么就是协变，如果关系反转了就是逆变，如果没有任何类型关系就是不变。


假定`F<T>`中F是类型构造器，T是组件类型，那么型变通常包含三类关系，如下:

* `F<Sub>`是`F<Super>`的子类型，这种关系我们称之为协变
* `F<Super>`是`F<Sub>`的子类型，这种关系我们称之为逆变
* `F<Super`和`F<Sub`没有任何关系，我们称之为不变

!!! Tips
    类型构造器的组件类型可以有多个比如`F<T, U>`，在判断型变关系的时候需要非别对各个组件类型来讨论，比如对于T是协变、对于U是不变等。


!!! Note
    通过上面对Variance的定义可以看出，型变的前提是类型之间存在关系，而这个关系通常来说指的就是子类型关系(也可以是其他关系)，一个类型是另外一个类型的子类型，一般来说这是通过继承来实现的。
    这种关系并非只有子类型，在接下来的文章中我们会看到Rust中的生命周期也是一种类似子类型的关系。

# Rust Variance

Rust中的类型不存子类型的关系，因为他没有类型的继承，自然也就没有所谓的子类型关系了。但是Rust也是有型变的，它的型变指的是生命周期的型变，虽然他的类型之间没有关系，但是他的生命周期存在子类型关系。
也就是大的生命周期是小生命周期的子类型。也就是说我可以用一个小生命周期的变量指向一个更大的生命周期变量。


!!! Note
    `'static`是所有生命周期的子类型


在Variance的定义中我们曾经提到类型构造器，在Rust中有泛型、还有`&`、`&mut`等，这些都是类型构造器，下面这张表格是Rust中各种类型构造器结合生命周期后产生的关系：


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


!!! Tips
    带`*`号的是重点关注的类型，也是最基本的类型，所有其他的类型都可以通过这些基本类型推导来理解。


对于`&'a T`来说，对于`'a`来说是协变、对于`T`也是协变。接着我们看一些例子来理解Rust中的型变。

```rust
fn evil_feeder(pet: &mut Animal) {
    let spike: Dog = ...;

    // `pet` is an Animal, and Dog is a subtype of Animal,
    // so this should be fine, right..?
    *pet = spike;
}

fn main() {
    let mut mr_snuggles: Cat = ...;
    evil_feeder(&mut mr_snuggles);  // Replaces mr_snuggles with a Dog
    mr_snuggles.meow();             // OH NO, MEOWING DOG!
}
```

上面的这个例子是无法编译的，因为`&mut Animal`对于T来说是不变的(参考上面的表格)。接着我们看下面这个例子:

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

`evil_feeder`中的第一个参数是`&mut T`，对于T来说是不变的，当传入`&mut mr_snuggles`时，也就是说T为`&'static str`，因为第二个参数的类型也是T，因此
 第二个参数实际上也是`&'static str`类型，这是针对`'static`的协变，但是`'static`是任何类型的子类型，只有`'static`才是`'static`的子类型，当我们传入非`'static`
 生命周期参数的时候总是无法满足子类型的需求，因此编译会出错。

 ```rust
 6  |     let mut mr_snuggles: &'static str = "meow! :3";  // mr. snuggles forever!!
   |                          ------------ type annotation requires that `spike` is borrowed for `'static`
...
9  |         let spike_str: &str = &spike;                // Only lives for the block
   |                               ^^^^^^ borrowed value does not live long enough
10 |         evil_feeder(& mut mr_snuggles, spike_str);    // EVIL!
11 |     }
   |     - `spike` dropped here while still borrowed
 ```

如果一个结构体有多个字段，多个字段

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