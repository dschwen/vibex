#include <cassert>
#include <vector>

#include "et/ast.hpp"
#include "et/normalize_ast.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-12) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

int main() {
  {
    // Flatten and sort Add; fold constants; drop zeros (AST)
    auto x = var(0), y = var(1), z = var(2);
    Expr e = ((x + (y + z)) + lit(0.0)) + (lit(2.0) + lit(3.0));
    Expr n = normalize(e);
    // Expect structure equivalent to x + y + z + 5
    double val = eval(n, {1.0, 2.0, 3.0});
    assert(approx(val, 1.0 + 2.0 + 3.0 + 5.0));
  }

  {
    // Flatten and sort Mul; drop ones; annihilator zero (AST)
    auto x = var(0), y = var(1);
    Expr e = (x * (lit(1.0) * y)) * lit(1.0);
    Expr n = normalize(e);
    double val = eval(n, {2.0, 3.0});
    assert(approx(val, 2.0 * 3.0));
    // Annihilator test: (x * 0 * y) -> 0
    Expr e0 = x * lit(0.0) * y;
    Expr n0 = normalize(e0);
    assert(approx(eval(n0, {2.0, 3.0}), 0.0));
  }

  {
    // Simple neutral rules for Sub/Div (AST)
    auto x = var(0);
    Expr n1 = normalize(x - lit(0.0));
    assert(approx(eval(n1, {3.2}), 3.2));
    Expr n2 = normalize(lit(0.0) / x);
    assert(approx(eval(n2, {3.2}), 0.0));
    Expr n3 = normalize(x / lit(1.0));
    assert(approx(eval(n3, {5.6}), 5.6));
  }

  return 0;
}
