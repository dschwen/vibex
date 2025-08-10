#include <iostream>
#include "et/expr.hpp"

int main() {
  using namespace et;
  auto [x,y,z] = Vars<double,3>();
  auto f = sin(x)*y + z*z;

  double val = evaluate(f, 2.4, 6.0, 1.5);
  std::cout << "f(2.4, 6, 1.5) = " << val << "\n";
  return 0;
}
