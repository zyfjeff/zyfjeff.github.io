/// nothing ⇒ Display
/// ? ⇒ Debug
/// x? ⇒ Debug with lower-case hexadecimal integers
/// X? ⇒ Debug with upper-case hexadecimal integers
/// o ⇒ Octal
/// x ⇒ LowerHex
/// X ⇒ UpperHex
/// p ⇒ Pointer
/// b ⇒ Binary
/// e ⇒ LowerExp
/// E ⇒ UpperExp

fn main() {
  println!("{0}, this is a {1}, {2}", "Alice", "Bob", "Ces");
  println!("{}", "Hello World");
  println!("{A}, {B}, {C}", A="A", B="B", C="C");
  println!("{:b} is binary, {:o} is octa", 10, 10); 
  println!("{number:0width$}", number = 1, width = 6);
  println!("float:{}", format!("{:.*}", 2, 1.2345678));
	println!("0011 AND 0101 is {:04b}", 0b0011u32 & 0b0101);
  println!("0x80 >> 2 is 0x{:x}", 0x80u32 >> 2);
  let tuple_of_tuples = ((1u8, 2u16, 2u32), (4u64, -1i8), -2i16);
  println!("pair: is {:?}", tuple_of_tuples);
}
