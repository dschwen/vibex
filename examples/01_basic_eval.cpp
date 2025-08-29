#include <iostream>
#include <vector>
#include "et/ast.hpp"

int main() {
  using namespace et;
  auto x = var(0);
  auto y = var(1);
  auto z = var(2);
  Expr f = sin(x) * y + z * z;

  std::vector<double> inputs = {2.4, 6.0, 1.5};
  double val = eval(f, inputs);
  std::cout << "f(2.4, 6, 1.5) = " << val << "\n";
  return 0;
}
