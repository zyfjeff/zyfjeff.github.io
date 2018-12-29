fn main() {
 let outer = 42;
 {
   let inner = 3.14;
   println!("block variable: {}", inner);
   let outer = 99;
   println!("block variable outer: {}", outer);
 }

 println!("outer variable: {}", outer);
}
