#include <cassert>
#include <vector>
#include <cmath>

#include "et/ast.hpp"
#include "et/normalize_ast.hpp"
#include "et/rewrite_ast.hpp"

using namespace et;

int main() {
  {
    auto x = var(0);
    Expr e = sin(x)*sin(x) + cos(x)*cos(x);
    Expr r = rewrite_fixed_point(e);
    for (double t : {-3.0, -0.5, 0.0, 0.9, 1.7})
      assert(std::fabs(eval(r, {t}) - 1.0) < 1e-12);
  }

  {
    auto z = var(0);
    Expr e = log(exp(z));
    Expr r = rewrite_fixed_point(e);
    double v = eval(r, {3.14});
    assert(std::fabs(v - 3.14) < 1e-12);
  }

  return 0;
}
