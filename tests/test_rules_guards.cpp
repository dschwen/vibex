#include <cassert>
#include <string>

#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/normalize.hpp"
#include "et/rewrite.hpp"
#include "et/rules_default.hpp"

using namespace et;

int main() {
  auto [a,b] = Vars<double,2>();

  // Case: guard should NOT fire (coefficient 3 instead of required 2)
  auto e_bad = a*a + lit(3.0)*a*b + b*b;
  RGraph g0 = normalize(compile_to_runtime(e_bad));
  auto rules = default_rules();
  RGraph gr = rewrite_fixed_point(g0, rules, 8);
  gr = normalize(gr);
  std::string s = r_to_string(gr);
  // No square completion expected -> no Pow(..., C(2)) introduced
  assert(s.find("Pow(") == std::string::npos);

  // Case: guard should fire (coefficient 2)
  auto e_ok = a*a + lit(2.0)*a*b + b*b;
  RGraph g1 = normalize(compile_to_runtime(e_ok));
  RGraph gr1 = rewrite_fixed_point(g1, rules, 8);
  gr1 = normalize(gr1);
  std::string s1 = r_to_string(gr1);
  // Expect a Pow(Add(a,b), C(2)) form
  assert(s1.find("Pow(Add(") != std::string::npos);
  assert(s1.find(",C(2))") != std::string::npos);

  return 0;
}

