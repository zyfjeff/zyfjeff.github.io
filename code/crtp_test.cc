#include "crtp.h"

#include <iostream>

class test : public AddOperator<test> {
 public:
  test(int value) : value_(value) {}
  int value() const { return value_; }
 private:
  int value_;
};

int main() {
  test t1(11), t2(20);
  std::cout << (t1 + t2).value() << std::endl;
  return 0;
}
