#include <cassert>
#include <vector>

#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/normalize.hpp"
#include "et/rewrite.hpp"
#include "et/rules_default.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-12) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

int main() {
  auto [x] = Vars<double,1>();
  auto e = sin(x)*sin(x) + cos(x)*cos(x) + (lit(2.0)*x + lit(3.0)*x);
  RGraph g = compile_to_runtime(e);
  RGraph gn0 = normalize(g);
  std::string before = r_to_string(gn0);
  auto rules = default_rules();
  RGraph gr = rewrite_fixed_point(g, rules, 12);
  gr = normalize(gr);
  std::string after = r_to_string(gr);
  // Expect structure changed and simplified to Add(C(1),Mul(C(5),V(0)))
  assert(before != after);
  assert(after == std::string("Add(C(1),Mul(C(5),V(0)))"));
  // Numeric equivalence
  double v = eval(gr, std::vector<double>{1.7});
  assert(approx(v, 1.0 + 5.0*1.7));
  return 0;
}
