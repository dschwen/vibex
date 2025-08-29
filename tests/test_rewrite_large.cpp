#include <cassert>
#include <vector>
#include <cmath>

#include "et/ast.hpp"
#include "et/normalize_ast.hpp"
#include "et/rewrite_ast.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-12) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

int main() {
  auto x = var(0);
  Expr e = sin(x)*sin(x) + cos(x)*cos(x) + (lit(2.0)*x + lit(3.0)*x);
  Expr r = rewrite_fixed_point(e);
  // Numeric equivalence to 1 + 5*x
  for (double xv : { -1.3, 0.0, 0.7, 2.2 })
    assert(approx(eval(r, {xv}), 1.0 + 5.0 * xv));
  return 0;
}
