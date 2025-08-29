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
  auto a = var(0), b = var(1);

  // Guard should NOT trigger for coeff 3: result should not equal (a+b)^2
  {
    Expr e_bad = a*a + lit(3.0)*a*b + b*b;
    Expr r_bad = rewrite_fixed_point(e_bad);
    std::vector<double> in = {1.1, 0.7};
    double v_bad = eval(r_bad, in);
    double ref_sq = (in[0]+in[1])*(in[0]+in[1]);
    assert(!approx(v_bad, ref_sq));
  }

  // Guard should trigger for coeff 2: equals (a+b)^2
  {
    Expr e_ok = a*a + lit(2.0)*a*b + b*b;
    Expr r_ok = rewrite_fixed_point(e_ok);
    std::vector<double> in = {1.3, -0.4};
    double v_ok = eval(r_ok, in);
    double ref_sq = (in[0]+in[1])*(in[0]+in[1]);
    assert(approx(v_ok, ref_sq));
  }

  return 0;
}
