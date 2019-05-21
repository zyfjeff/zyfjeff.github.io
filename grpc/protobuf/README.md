## protobuf3基础

```protobuf
syntax = "proto3" // 指定protobuf的版本，默认使用的proto2

message SearchRequest {
  string query = 1;
  int32 page_number = 2;
  int32 result_per_page = 3,
}
```

1. 每一个字段含有一个唯一的数字，用于标识这个字段在二进制流中的位置，一旦使用了，就不要修改这个数字，会导致不兼容。
  字段的数字范围在1~15的时候，占用一个字节，字段的数字范围在16~2017占用二个字节。
2. 每一个字段都有一个类型和一个名字
3. 字段的数字范围在19000 到 19999为保留的数字(FieldDescriptor::kFirstReservedNumber through FieldDescriptor::kLastReservedNumber)不能使用
4. 使用repeated修饰的标量类型默认使用`packed`来编码
5.

##