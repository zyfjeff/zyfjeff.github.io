#include <vector>
#include <iostream>
#include <string>
#include <cstddef>
#include <initializer_list>

namespace reflect {

struct TypeDescriptor {
  const char* name_;
  size_t size_;
  TypeDescriptor(const char* name, size_t size) : name_(name), size_(size) {}
  virtual ~TypeDescriptor() {}
  virtual std::string GetFullName() const { return name_; }
  virtual void Dump(const void* obj, int indent_level = 0) const = 0;
};


template <typename T>
TypeDescriptor* GetPrimitiveDescriptor();

struct DefaultResolver {
  template <typename T> static char func(decltype(&T::Reflection));
  template <typename T> static int func(...);
  template <typename T>
  struct IsReflected {
    enum { value = (sizeof(func<T>(nullptr)) == sizeof(char))};
  };

  // enable_if 自定义判断条件
  template <typename T, typename std::enable_if<IsReflected<T>::value, int>::type = 0>
  static TypeDescriptor* get() {
    return &T::Reflection;
  }

  template <typename T, typename std::enable_if<!IsReflected<T>::value, int>::type = 0>
  static TypeDescriptor* get() {
    return GetPrimitiveDescriptor<T>();
  }
};

// 泛化对应类型的TypeDescriptor结构
template <typename T>
struct TypeResolver {
  static TypeDescriptor* get() {
    // 根据类型判断调用的是哪个
    return DefaultResolver::get<T>();
  }
};


struct TypeDescriptor_Struct : TypeDescriptor {
  struct Member {
    const char* name_;
    size_t offset_;
    TypeDescriptor* type_;
  };

  std::vector<Member> members_;

  TypeDescriptor_Struct(void (*init)(TypeDescriptor_Struct*)) : TypeDescriptor(nullptr, 0) {
    init(this);
  }

  TypeDescriptor_Struct(const char* name, size_t size, const std::initializer_list<Member>& init)
    : TypeDescriptor{nullptr, 0}, members_{init} { }

  virtual void Dump(const void* obj, int indent_level) const override {
    for (const Member& member : members_) {
        std::cout << std::string(4 * (indent_level + 1), ' ') << member.name_ << " = ";
        member.type_->Dump((char*) obj + member.offset_, indent_level + 1);
        std::cout << std::endl;
    }
    std::cout << std::string(4 * indent_level, ' ') << "}";
  }
};


#define REFLECT() \
  friend struct reflect::DefaultResolver; \
  static reflect::TypeDescriptor_Struct Reflection; \
  static void InitReflection(reflect::TypeDescriptor_Struct*);

#define REFLECT_STRUCT_BEGIN(type)  \
  reflect::TypeDescriptor_Struct type::Reflection{type::InitReflection}; \
  void type::InitReflection(reflect::TypeDescriptor_Struct* type_desc) {  \
    using T = type; \
    type_desc->name_ = #type;  \
    type_desc->size_ = sizeof(T);  \
    type_desc->members_ = {


#define REFLECT_STRUCT_MEMBER(name) \
        {#name, offsetof(T, name), reflect::TypeResolver<decltype(T::name)>::get()},

#define REFLECT_STRUCT_END() \
    };  \
  }


struct TypeDescriptor_StdVector : TypeDescriptor {
  TypeDescriptor* item_type_;
  size_t(*getSize)(const void*);
  const void* (*getItem)(const void*, size_t);

  template <typename ItemType>
  TypeDescriptor_StdVector(ItemType*)
    : TypeDescriptor{"std::vector<>", sizeof(std::vector<ItemType>)},
      item_type_{TypeResolver<ItemType>::get()} {
        getSize = [](const void* vecPtr) -> size_t {
            const auto& vec = *(const std::vector<ItemType>*) vecPtr;
            return vec.size();
        };
        getItem = [](const void* vecPtr, size_t index) -> const void* {
            const auto& vec = *(const std::vector<ItemType>*) vecPtr;
            return &vec[index];
        };
      }

      virtual std::string GetFullName() const override {
        return std::string("std::vector<") + item_type_->GetFullName() + ">";
      }

      virtual void Dump(const void* obj, int indentLevel) const override {
        size_t numItems = getSize(obj);
        std::cout << GetFullName();
        if (numItems == 0) {
            std::cout << "{}";
        } else {
            std::cout << "{" << std::endl;
            for (size_t index = 0; index < numItems; index++) {
                std::cout << std::string(4 * (indentLevel + 1), ' ') << "[" << index << "] ";
                item_type_->Dump(getItem(obj, index), indentLevel + 1);
                std::cout << std::endl;
            }
            std::cout << std::string(4 * indentLevel, ' ') << "}";
        }
    }
};

// Partially specialize TypeResolver<> for std::vectors:
template <typename T>
class TypeResolver<std::vector<T>> {
public:
    static TypeDescriptor* get() {
        static TypeDescriptor_StdVector typeDesc{(T*) nullptr};
        return &typeDesc;
    }
};

}  // namespace reflect