#include <iostream>
#include "et/expr.hpp"

int main() {
  using namespace et;
  auto [x,y,z] = Vars<double,3>();
  auto f = sin(x)*y + z*z;

  auto dfdx = diff(f, x);
  auto dfdy = diff(f, y);
  auto dfdz = diff(f, z);

  std::cout << "df/dx at (2.4,6,1.5) = " << evaluate(dfdx, 2.4, 6.0, 1.5) << "\n";
  std::cout << "df/dy at (2.4,6,1.5) = " << evaluate(dfdy, 2.4, 6.0, 1.5) << "\n";
  std::cout << "df/dz at (2.4,6,1.5) = " << evaluate(dfdz, 2.4, 6.0, 1.5) << "\n";
  return 0;
}
