#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"

using namespace et;

int main() {
  auto [x] = Vars<double,1>();

  // If: piecewise quadratic/linear
  auto y_if = If(x > lit(0.0), x * x, x);
  assert(y_if(2.0) == 4.0);
  assert(y_if(-3.0) == -3.0);

  // Select: abs(x) using where
  auto y_sel = Select(x >= lit(0.0), x, -x);
  assert(y_sel(3.0) == 3.0);
  assert(y_sel(-2.5) == 2.5);

  // Nested: If over Select
  auto z = If(x < lit(1.0), y_sel, x + lit(1.0));
  assert(z(-5.0) == 5.0);
  assert(z(2.0) == 3.0);
  return 0;
}

