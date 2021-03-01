---
hide:
  - toc        # Hide table of contents
---

# 常见的生命周期错误示例

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


**解决方案**

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

* 示例3

```rust

```

https://github.com/rust-lang/rfcs/blob/master/text/2094-nll.md
https://github.com/pretzelhammer/rust-blog/blob/master/posts/translations/zh-hans/common-rust-lifetime-misconceptions.md