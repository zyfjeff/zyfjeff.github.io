核心的数据结构是 `TypeDescriptor`，用来描述一个类型:

```C++
struct TypeDescriptor {
  const char* name_;
  size_t size_;
  TypeDescriptor(const char* name, size_t size) : name_(name), size_(size) {}
  virtual ~TypeDescriptor() {}
  virtual std::string GetFullName() const { return name_; }
  virtual void Dump(const void* obj, int indent_level = 0) const = 0;
};
```

然后通过`enable_if`的手法来区分出基本类型还是聚合类型，对于聚合类型直接返回聚合类型中定义的`Reflection`结构
对于基本类型，直接调用`GetPrimitiveDescriptor`获取。然后通过宏来完成反射数据结构的定义、初始化等。
