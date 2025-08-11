#include <cassert>
#include <vector>

#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/normalize.hpp"
#include "et/pattern.hpp"
#include "et/match.hpp"
#include "et/rewrite.hpp"
#include "et/rules_default.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-12) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

int main() {
  using namespace et::pat;
  // Plus square
  {
    auto [a,b] = Vars<double,2>();
    auto e = a*a + lit(2.0)*a*b + b*b + a; // include extra term to exercise spread
    auto rules = default_rules();
    RGraph g = compile_to_runtime(e);
    RGraph gr = rewrite_fixed_point(g, rules, 10);
    gr = normalize(gr);
    // Check numerically
    std::vector<double> in = {1.3, 0.7};
    double v = eval(gr, in);
    double ref = (in[0]+in[1])*(in[0]+in[1]) + in[0];
    assert(approx(v, ref));
    // Check structure matches (a+b)^2 + rest
    Pattern fact = add( pow(add(P(1),P(2)), C(2.0)), S(9) );
    Bindings bnd; MultiBindings mb;
    bool ok = match(gr, fact, bnd, mb);
    assert(ok);
  }
  // Minus square
  {
    auto [a,b] = Vars<double,2>();
    auto e = a*a - lit(2.0)*a*b + b*b + b;
    auto rules = default_rules();
    RGraph g = compile_to_runtime(e);
    RGraph gr = rewrite_fixed_point(g, rules, 10);
    gr = normalize(gr);
    std::vector<double> in = {2.0, 0.25};
    double v = eval(gr, in);
    double ref = (in[0]-in[1])*(in[0]-in[1]) + in[1];
    assert(approx(v, ref));
    Pattern fact = add( pow(sub(P(1),P(2)), C(2.0)), S(9) );
    Bindings bnd; MultiBindings mb;
    bool ok = match(gr, fact, bnd, mb);
    assert(ok);
  }
  return 0;
}
