#include "strong_type.h"

#include <iostream>
#include <string>

using Width = NamedType<double, struct WidthParameter>;
using Height = NamedType<double, struct HeightParameter>;
using HeightRef = NamedType<double&, struct HeightRefParameterR>;

using FirstNameRef = NamedTypeRef<std::string, struct FirstNameRefParameter>;

void test(Width w, Height h) {}

template<typename Function>
struct Comparator : NamedType<Function, Comparator<Function>>
{
    using NamedType<Function, Comparator<Function>>::NamedType;
};

template<typename Function>
struct Aggregator : NamedType<Function, Aggregator<Function>>
{
    using NamedType<Function, Aggregator<Function>>::NamedType;
};

template<typename Function1, typename Function2>
void set_aggregate(Function1 c, Function2 a)
{
   std::cout << "Compare: " << c.get()() << std::endl;
   std::cout << "Aggregate: " << a.get()() << std::endl;
}

int main() {
  std::string t("test");
  double d = 1.0;
  test(Width(1.0), Height(2.0));
  HeightRef rd(d);
  FirstNameRef rt(t);

  set_aggregate(make_named<Comparator>([](){ return "compare"; }), make_named<Aggregator>([]() { return "aggregate"; }));
}
