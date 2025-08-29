#include <cassert>
#include <cmath>
#include <vector>
#include "et/ast.hpp"

using namespace et;

int main() {
  auto x = var(0);

  // If: piecewise quadratic/linear
  Expr y_if = If(x > lit(0.0), x * x, x);
  assert(std::abs(eval(y_if, {2.0}) - 4.0) < 1e-12);
  assert(std::abs(eval(y_if, {-3.0}) + 3.0) < 1e-12);

  // Select: abs(x) using where
  Expr y_sel = Select(x >= lit(0.0), x, -x);
  assert(std::abs(eval(y_sel, {3.0}) - 3.0) < 1e-12);
  assert(std::abs(eval(y_sel, {-2.5}) - 2.5) < 1e-12);

  // Nested: If over Select
  Expr z = If(x < lit(1.0), y_sel, x + lit(1.0));
  assert(std::abs(eval(z, {-5.0}) - 5.0) < 1e-12);
  assert(std::abs(eval(z, {2.0}) - 3.0) < 1e-12);
  return 0;
}
