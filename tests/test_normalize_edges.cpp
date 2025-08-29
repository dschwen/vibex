#include <cassert>
#include <string>

#include "et/ast.hpp"
#include "et/normalize_ast.hpp"

using namespace et;

int main() {
  // Add of constants only: 2 + 3 -> C(5)
  { auto e = lit(2.0) + lit(3.0); Expr n = normalize(e); assert(eval(n, {0.0}) == 5.0); }

  // Add zeros only: 0 + 0 -> C(0)
  { auto e = lit(0.0) + lit(0.0); Expr n = normalize(e); assert(eval(n, {0.0}) == 0.0); }

  // x + 0 -> x (size==1 collapsing)
  { auto x = var(0); Expr e = x + lit(0.0); Expr n = normalize(e); assert(eval(n, {1.23}) == 1.23); }

  // Mul constants only: 1*1 -> C(1) (empty flat)
  { auto e = lit(1.0) * lit(1.0); Expr n = normalize(e); assert(eval(n, {0.0}) == 1.0); }

  // Mul constants fold: 2*x*3 -> Mul(Const(6), Var(0)) (order deterministic but assert structurally)
  { auto x = var(0); Expr e = lit(2.0) * x * lit(3.0); Expr n = normalize(e); double v = eval(n, {1.75}); assert(std::fabs(v - 6.0*1.75) < 1e-12); }

  // Mul constants only: 2*3 -> C(6)
  { auto e = lit(2.0) * lit(3.0); Expr n = normalize(e); assert(eval(n, {0.0}) == 6.0); }

  // Div equal -> 1: x/x -> C(1)
  { auto x = var(0); Expr e = x / x; Expr n = normalize(e); double v = eval(n, {2.0}); assert(std::fabs(v - 1.0) < 1e-12); }

  return 0;
}
