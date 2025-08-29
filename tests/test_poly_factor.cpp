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
  // Plus square
  {
    auto a = var(0), b = var(1);
    Expr e = a*a + lit(2.0)*a*b + b*b + a; // include extra term
    Expr r = rewrite_fixed_point(e);
    std::vector<double> in = {1.3, 0.7};
    double v = eval(r, in);
    double ref = (in[0]+in[1])*(in[0]+in[1]) + in[0];
    assert(approx(v, ref));
  }
  // Minus square
  {
    auto a = var(0), b = var(1);
    Expr e = a*a - lit(2.0)*a*b + b*b + b;
    Expr r = rewrite_fixed_point(e);
    std::vector<double> in = {2.0, 0.25};
    double v = eval(r, in);
    double ref = (in[0]-in[1])*(in[0]-in[1]) + in[1];
    assert(approx(v, ref));
  }
  return 0;
}
