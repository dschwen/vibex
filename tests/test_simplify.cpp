// AST constant/numeric evaluation sanity (no compile-time simplify)
#include <cassert>
#include <cmath>
#include <vector>
#include "et/ast.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-12) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

int main() {
  // Unary constant expressions: numeric checks
  {
    auto e1 = exp(lit(0.2));
    auto e2 = log(lit(1.7));
    auto e3 = sqrt(lit(2.5));
    auto e4 = tanh(lit(-0.9));
    auto e5 = -lit(3.0);
    assert(approx(eval(e1, {0.0}), std::exp(0.2)));
    assert(approx(eval(e2, {0.0}), std::log(1.7)));
    assert(approx(eval(e3, {0.0}), std::sqrt(2.5)));
    assert(approx(eval(e4, {0.0}), std::tanh(-0.9)));
    assert(approx(eval(e5, {0.0}), -3.0));
  }

  // Non-constant unary: evaluation preserved
  {
    auto x = var(0);
    auto e = exp(x);
    double xv = 0.3;
    assert(approx(eval(e, {xv}), std::exp(xv)));
  }

  // Binary constant expressions: numeric checks
  {
    auto a = lit(2.0) + lit(5.0);
    auto b = lit(6.0) - lit(1.5);
    auto c = lit(3.0) * lit(4.0);
    auto d = lit(9.0) / lit(2.0);
    assert(approx(eval(a, {0.0}), (2.0 + 5.0)));
    assert(approx(eval(b, {0.0}), (6.0 - 1.5)));
    assert(approx(eval(c, {0.0}), (3.0 * 4.0)));
    assert(approx(eval(d, {0.0}), (9.0 / 2.0)));
  }

  // Binary non-constant: evaluation preserved
  {
    auto x = var(0), y = var(1);
    auto e = x + lit(3.0);
    double xv = 1.2, yv = -0.7;
    assert(approx(eval(e, {xv, yv}), xv + 3.0));
  }

  return 0;
}
