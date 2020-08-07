
CFG(Context-free grammar)上下文无法语法，PEG(Parsing expression grammar)表达式解析语法， 这是两种文法形式。
BNF、EBNF属于CFG文法的两种实现。


PEG来描述语法(pest):

Tokens = {}

SOI 开始
EOI 结束



Parser的结果可以通过tokens获取到所有的token

```rust
let parse_result = Parser::parse(Rule::sum, "1773 + 1362").unwrap();
let tokens = parse_result.tokens();

// 输出1773 + 1362三个token
for token in tokens {
    println!("{:?}", token);
}
```

每一个token包含两个字段:

* rule, which explains which rule generated them; and
* pos, which indicates their positions.


Token使用起来还是有些不方便，Pest提供了pair

A Pair represents a matching pair of tokens, or, equivalently,
the spanned text that a named rule successfully matched. It is commonly used in several ways:

* Determining which rule produced the Pair
* Using the Pair as a raw &str
* Inspecting the inner named sub-rules that produced the Pair


```rust
let pair = Parser::parse(Rule::enclosed, "(..6472..) and more text")
    .unwrap().next().unwrap();

assert_eq!(pair.as_rule(), Rule::enclosed);
assert_eq!(pair.as_str(), "(..6472..)");

let inner_rules = pair.into_inner();
println!("{}", inner_rules); // --> [number(3, 7)]
```


Example:

```pest
field = { (ASCII_DIGIT | "." | "-")+ }
record = { field ~ ("," ~ field)* }
file = { SOI ~ (record ~ ("\r\n" | "\n"))* ~ EOI }
```


Reference:

* https://en.wikipedia.org/wiki/Parsing_expression_grammar

