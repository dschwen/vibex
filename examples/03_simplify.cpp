#include <iostream>
#include "et/expr.hpp"
#include "et/simplify.hpp"

int main() {
  using namespace et;
  auto [x,y,z] = Vars<double,3>();
  auto f = sin(x)*y + z*z + lit(0.0);
  auto dfdx = diff(f, x);
  auto dfdx_s = simplify(dfdx);

  std::cout << "df/dx at (2.4,6,1.5) = " << evaluate(dfdx, 2.4, 6.0, 1.5) << "\n";
  std::cout << "df/dx simplified at same point = " << evaluate(dfdx_s, 2.4, 6.0, 1.5) << "\n";
  return 0;
}
