# Rust学习笔记
## Type

* `from`

实现From traits，

```rust
#[derive(Debug, Clone, Copy, PartialEq)]
struct Vec2 {
    x: i64,
    y: i64,
}
impl From<(i64, i64)> for Vec2 {
    fn from((x, y): (i64, i64)) -> Self {
        Self { x, y }
    }
}

#[test]
fn test_tuple() {
    // 自动调用from trait
    let v: Vec2 = (5, 8).into();
    assert_eq!(v.x, 5);
    assert_eq!(v.y, 8);
}
```

* `Default`

```rust
#[derive(Clone, Copy, PartialEq)]
enum Tile {
    Open,
    Tree,
}
impl Default for Tile {
    fn default() -> Self {
        Self::Open
    }
}

let tile : Tile = Default::default();
```

## Option

* `map`
* `unwrap_or_default`


## Iterator

三类迭代器，move、shared reference、mutable reference，for循环默认使用`into_iter`，也就是move迭代器。

* `into_iter` 
* `iter`
* `iter_mut`


迭代器有很多适配器，通过这些适配器可以很方便的进行一些计算，这些适配器返回的都是另外一个迭代器。

* `copied` 针对iter类型的迭代器进行复制

> into_iter的迭代器赋值没有意思，本身是move的，再Copy一次也没有意义

* `map` 接受一个func，并以func结果作为另外一个迭代器的值。

* `collect` 将迭代器转换为collection

* `enumerate` 返回一个新的迭代器，迭代器的值是一个tuple，第一个值是index，第二值是迭代器的值。

* `filter_map` 返回值类型是Option的迭代器，迭代的时候只返回非None的值。

* `lines` 按行切割，返回一个迭代器

* `count` 返回迭代器值的数量

* `product` 迭代器中的元素相乘，得到一个最后的结果

`itertools` crate提供了更丰富的迭代器

* `tuple_combinations` 返回一个迭代器适配器，该适配器遍历迭代器中元素的组合。

实现迭代器




## String


## Third party

* `itertools`

提供了丰富的迭代器，比如上面提到的`tuple_combinations`迭代适配器。

* `peg`

可以用来做一些parser，example如下:

```rust
// 1-3 a: abcde
// 1-3 b: cdefg
// 2-9 c: ccccccccc

fn parse_line(s: &str) -> anyhow::Result<(PasswordPolicy, &str)> {
    peg::parser! {
        grammar parser() for str {
            // 先定义各种rule，用来匹配一些基本元素，比如这里的number、range、byte、password等
            // 最后用这些基本的rule来描述最终要解析的行，以此来提取这些基本元素。
            rule number() -> usize
                = n:$(['0'..='9']+) { n.parse().unwrap() }
            rule range() -> RangeInclusive<usize>
                = min:number() "-" max:number() { min..=max }
            rule byte() -> u8
                = letter:$(['a'..='z']) { letter.as_bytes()[0] }
            rule password() -> &'input str
                = letters:$([_]*) { letters }
            pub(crate) rule line() -> (PasswordPolicy, &'input str)
                = range:range() " " byte:byte() ": " password:password() {
                    (PasswordPolicy { range, byte }, password)
                }
        }
    }

    Ok(parser::line(s)?)
}
```

* `pest`

* `thiserror`

* `anyhow`