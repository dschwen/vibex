#include <cassert>

#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/normalize.hpp"
#include "et/pattern.hpp"
#include "et/match.hpp"
#include "et/rewrite.hpp"
#include "et/rules_default.hpp"

using namespace et;

int main() {
  {
    auto [x] = Vars<double,1>();
    auto e = sin(x)*sin(x) + cos(x)*cos(x);
    auto rules = default_rules();
    RGraph g = compile_to_runtime(e);
    g = normalize(g);
    RGraph g2 = rewrite_fixed_point(g, rules, 3);
    g2 = normalize(g2);
    const RNode& n = g2.nodes[g2.root];
    assert(n.kind == NodeKind::Const && n.cval == 1.0);
  }

  {
    auto [z] = Vars<double,1>();
    auto e = log(exp(z));
    auto rules = default_rules();
    RGraph g = rewrite_expr(e, rules);
    double v = eval(g, std::vector<double>{3.14});
    assert(v == 3.14);
  }

  return 0;
}
