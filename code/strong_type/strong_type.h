#include <memory>

template <typename T, typename Parameter>
class NamedType {
 public:
  explicit NamedType(const T& value) : value_(value) {}
  template<typename T_ = T>
  explicit NamedType(T&& value, typename std::enable_if<!std::is_reference<T_>{},
                     std::nullptr_t>::type = nullptr)
    : value_(std::move(value)) {}
  T& get() { return value_; }
  T const& get() const { return value_; }
 private:
  T value_;
};

template <typename T, typename Parameter>
class NamedTypeRef {
 public:
  explicit NamedTypeRef(T& t) : t_(std::ref(t)) {}
  T& get() { return t_.get(); }
  const T& get() const { return t_.get(); }

 private:
  std::reference_wrapper<T> t_;
};

template<template<typename T> class GenericTypeName, typename T>
GenericTypeName<T> make_named(T const& value)
{
    return GenericTypeName<T>(value);
}
