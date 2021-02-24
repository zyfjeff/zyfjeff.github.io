## 模式

你可以在Rust中很多地方看到模式的身影，模式是Rust提供的一种特殊语法， 它是用来匹配类型中的结构，无论类型是简单还是复杂。怎么理解这句话呢? 

1. match

```
match VALUE {
    PATTERN => EXPRESSION,
    PATTERN => EXPRESSION,
    PATTERN => EXPRESSION,
}
```