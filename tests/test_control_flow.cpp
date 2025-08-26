#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/runtime_ast.hpp"

using namespace et;

int main() {
  auto [x] = Vars<double,1>();

  // If(x > 0, then, else)
  auto expr1 = If(x > lit(0.0), x + lit(1.0), x - lit(1.0));
  auto g1 = compile_to_runtime(expr1);
  assert(eval(g1, {0.0}) == -1.0);
  assert(eval(g1, {5.0}) == 6.0);

  // Derivative mirrors structure and ignores mask derivative
  auto d1 = diff(expr1, x);
  auto g1d = compile_to_runtime(d1);
  // For x=5 (>0): derivative of then branch (x+1) is 1
  assert(eval(g1d, {5.0}) == 1.0);
  // For x=0 (false): derivative of else branch (x-1) is 1 as well
  assert(eval(g1d, {0.0}) == 1.0);

  // Basic comparisons
  auto c_lt = x < lit(2.0);
  auto c_ge = x >= lit(2.0);
  auto c_eq = x == lit(2.0);
  auto c_not = !c_eq;
  auto glt = compile_to_runtime(c_lt);
  auto gge = compile_to_runtime(c_ge);
  auto geq = compile_to_runtime(c_eq);
  auto gnot = compile_to_runtime(c_not);
  assert(eval(glt, {1.0}) == 1.0);
  assert(eval(gge, {1.0}) == 0.0);
  assert(eval(geq, {2.0}) == 1.0);
  assert(eval(gnot, {2.0}) == 0.0);

  // Select(mask, a, b)
  auto expr2 = Select(lit(1.0), lit(3.0), x);
  auto g2 = compile_to_runtime(expr2);
  assert(eval(g2, {42.0}) == 3.0);

  // d/dx Select(1, 3, x) = Select(1, 0, 1) -> 0
  auto d2 = diff(expr2, x);
  auto g2d = compile_to_runtime(d2);
  assert(eval(g2d, {7.0}) == 0.0);

  return 0;
}
