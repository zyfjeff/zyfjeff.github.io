## clap


* Setp1: 设置App name、version、author、about等

```rust
let matches = App::new("MyApp")
                    .version("1.0")
                    .author("Kevin K. <kbknapp@gmail.com>")
                    .about("Does awesome things")
```

* Setp2:



## serde

```rust
[dependencies]
# 需要开启derive
serde = {version = "1.0.102",  features = ["derive"]}
serde_json = "1.0.41"
serde_derive = "1.0.102"
```

1. 把json字符串转换为json对象(untyped)或者是转换为struct

```rust
let data = r#"
    {
        "name": "John Doe",
        "age": 43,
        "phones": [
            "+44 1234567",
            "+44 2345678"
        ]
    }"#;

// Parse the string of data into serde_json::Value.
let v: Value = serde_json::from_str(data)?;
```

```rust
#[macro_use]
use serde::{Deserialize, Serialize};
use serde_json::Result;
use serde_json;

#[derive(Serialize, Deserialize)]
struct Person {
    name: String,
    age: u8,
    phones: Vec<String>,
}

fn typed_example() -> Result<()> {
    // Some JSON input data as a &str. Maybe this comes from the user.
    let data = r#"
        {
            "name": "John Doe",
            "age": 43,
            "phones": [
                "+44 1234567",
                "+44 2345678"
            ]
        }"#;

    // Parse the string of data into a Person object. This is exactly the
    // same function as the one that produced serde_json::Value above, but
    // now we are asking it for a Person as output.
    let p: Person = serde_json::from_str(data)?;

    // Do things just like with any other Rust data structure.
    println!("Please call {} at the number {}", p.name, p.phones[0]);

    Ok(())
}

fn main() -> Result<()> {
  typed_example()
}
```

2. json宏转换json字符串为json对象

```rust
let john = json!({
    "name": "John Doe",
    "age": 43,
    "phones": [
        "+44 1234567",
        "+44 2345678"
    ]
});
```

3. 序列化一个struct到string

```rust
use serde::{Serialize, Deserialize};

#[derive(Serialize, Deserialize, Debug)]
struct Point {
    x: i32,
    y: i32,
}

fn main() {
    let point = Point { x: 1, y: 2 };

    let serialized = serde_json::to_string(&point).unwrap();
    println!("serialized = {}", serialized);

    let deserialized: Point = serde_json::from_str(&serialized).unwrap();
    println!("deserialized = {:?}", deserialized);
}
```

`#[serde(rename = "name")]` 字段重命名

## assert_cmd

```rust
extern crate assert_cmd;

use std::process::Command;
use assert_cmd::prelude::*;

Command::cargo_bin("bin_fixture")
    .unwrap()
    .assert()
    .success();
```

## predicates

提供一系列的算子组合



## std::collections

* `shrink_to_fit`内存收缩

* `extend` 内部调用`into_iter`会导致容器中的元素进行值值传递(移动)

```rust

fn main() {
  let mut vec1 = vec![1, 2, 3, 4];
  let vec2 = vec![10, 20, 30, 40];
  vec1.extend(vec2);

  // vec2已经被move了
  for x in vec2.iter_mut() {
    println!("vec contained {}", x);
    *x += 1;
  }
}
```

* `collect` 一种集合转换为另外一种集合的方式

```rust
use std::collections::VecDeque;
use std::collections::BinaryHeap;
use std::collections::BTreeSet;
use std::collections::HashSet;
use std::collections::LinkedList;

fn main() {
  let mut vec1 = vec![1, 2, 3, 4];
  let vec2 = vec![10, 20, 30, 40];
  vec1.extend(vec2);
  let vec = vec![1, 2, 3, 4];
  // let buf: VecDeque<_> = vec.into_iter().collect();
  // let buf: BinaryHeap<_> = vec.into_iter().collect();
  // let buf: BTreeSet<_> = vec.into_iter().collect();
  // let buf: HashSet<_> = vec.into_iter().collect();
  let buf: LinkedList<_> = vec.into_iter().collect();
}
```

* `rev`迭代器，可以逆向输出

* `entry` 很方便的操作map，插入一个key的时候，如果存在就插入，否则就不插入，会导致两次key的搜索，使用entry api可以避免这个问题

  1. 如果key存在就返回`Vacant(entry)`，可以insert
  2. 如果key不存在就返回`Occupied(entry)`，可以get、insert、remove

Additionally, they can convert the occupied entry into a mutable reference to its value, providing symmetry to the vacant insert case.

```rust
use std::collections::btree_map::BTreeMap;

let mut count = BTreeMap::new();
let message = "she sells sea shells by the sea shore";

for c in message.chars() {
    // 如果key存在就返回Vacant(entry)，可以插入
    // 如果key不存在就返回Occupied(entry)，可以get、insert、remove
    *count.entry(c).or_insert(0) += 1;
}

assert_eq!(count.get(&'s'), Some(&8));

println!("Number of occurrences of each character");
for (char, count) in &count {
    println!("{}: {}", char, count);
}
```

* `OccupiedEntry`
  1. `key`
  2. `remove_entry`
  3. `get`
  4. `get_mut`


* `BufRead`

```rust

pub trait Read {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize>;

    fn read_vectored(&mut self, bufs: &mut [IoSliceMut]) -> Result<usize> { ... }
    unsafe fn initializer(&self) -> Initializer { ... }
    fn read_to_end(&mut self, buf: &mut Vec<u8>) -> Result<usize> { ... }
    fn read_to_string(&mut self, buf: &mut String) -> Result<usize> { ... }
    fn read_exact(&mut self, buf: &mut [u8]) -> Result<()> { ... }
    fn by_ref(&mut self) -> &mut Self
    where
        Self: Sized,
    { ... }
    fn bytes(self) -> Bytes<Self>
    where
        Self: Sized,
    { ... }
    fn chain<R: Read>(self, next: R) -> Chain<Self, R>
    where
        Self: Sized,
    { ... }
    fn take(self, limit: u64) -> Take<Self>
    where
        Self: Sized,
    { ... }
}

pub trait BufRead: Read {
    fn fill_buf(&mut self) -> Result<&[u8]>;
    fn consume(&mut self, amt: usize);

    fn read_until(&mut self, byte: u8, buf: &mut Vec<u8>) -> Result<usize> { ... }
    fn read_line(&mut self, buf: &mut String) -> Result<usize> { ... }
    fn split(self, byte: u8) -> Split<Self>
    where
        Self: Sized,
    { ... }
    fn lines(self) -> Lines<Self>
    where
        Self: Sized,
    { ... }
}
```