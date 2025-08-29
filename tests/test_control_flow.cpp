#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/ast.hpp"

using namespace et;

int main() {
  auto x = var(0);

  // If(x > 0, then, else)
  Expr expr1 = If(x > lit(0.0), x + lit(1.0), x - lit(1.0));
  assert(eval(expr1, {0.0}) == -1.0);
  assert(eval(expr1, {5.0}) == 6.0);

  // Basic comparisons
  Expr c_lt = x < lit(2.0);
  Expr c_ge = x >= lit(2.0);
  Expr c_eq = (x == lit(2.0));
  Expr c_ne = (x != lit(2.0));
  Expr c_not = !c_eq;
  assert(eval(c_lt, {1.0}) == 1.0);
  assert(eval(c_ge, {1.0}) == 0.0);
  assert(eval(c_eq, {2.0}) == 1.0);
  assert(eval(c_ne, {2.0}) == 0.0);
  assert(eval(c_ne, {3.0}) == 1.0);
  assert(eval(c_not, {2.0}) == 0.0);

  // Select(mask, a, b)
  Expr expr2 = Select(lit(1.0), lit(3.0), x);
  assert(eval(expr2, {42.0}) == 3.0);
  return 0;
}
