#include "reflect.h"

namespace reflect {

struct TypeDescriptor_Int : TypeDescriptor {
  TypeDescriptor_Int() : TypeDescriptor{"int", sizeof(int)} {}

  virtual void Dump(const void* obj, int /* unused */) const override {
    std::cout << "int{" << *(const int*)obj << "}";
  }
};

template <>
TypeDescriptor* GetPrimitiveDescriptor<int>() {
    static TypeDescriptor_Int typeDesc;
    return &typeDesc;
}


struct TypeDescriptor_StdString : TypeDescriptor {
    TypeDescriptor_StdString() : TypeDescriptor{"std::string", sizeof(std::string)} {
    }
    virtual void Dump(const void* obj, int /* unused */) const override {
        std::cout << "std::string{\"" << *(const std::string*) obj << "\"}";
    }
};

template <>
TypeDescriptor* GetPrimitiveDescriptor<std::string>() {
    static TypeDescriptor_StdString typeDesc;
    return &typeDesc;
}

}  // namespace reflect
