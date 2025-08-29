#include <cassert>
#include <string>

#include "et/ast.hpp"
#include "et/normalize_ast.hpp"

using namespace et;

int main() {
  auto a = var(0), b = var(1), c = var(2);

  // 1) Basic: a - b -> a + (-b)
  {
    Expr e = a - b;
    Expr n = normalize(e);
    // Numeric check
    double v = eval(n, {3.0, 5.0, 0.0});
    assert(std::fabs(v - (3.0 - 5.0)) < 1e-12);
  }

  // 2) Mixed: a - (b - c) -> a - b + c
  {
    Expr e = a - (b - c);
    Expr n = normalize(e);
    double v = eval(n, {2.0, 7.0, 11.0});
    assert(std::fabs(v - (2.0 - (7.0 - 11.0))) < 1e-12);
  }

  // 3) Negation folds: -(-a) -> a; -Const -> Const(-)
  {
    Expr e = -( -a ) + lit(3.0) + ( -lit(2.0) );
    Expr n = normalize(e);
    double v = eval(n, {4.5, 0.0, 0.0});
    assert(std::fabs(v - (4.5 + 3.0 - 2.0)) < 1e-12);
  }

  return 0;
}
