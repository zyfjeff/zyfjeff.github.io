#include <iostream>

template <typename T>
struct AddOperator {
  T operator+ (const T& t) const {
    return T(t.value() + value());
  }

  int value() const { return static_cast<const T*>(this)->value(); }
};

