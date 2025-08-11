#include <cassert>
#include <unordered_map>

#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/normalize.hpp"
#include "et/pattern.hpp"
#include "et/match.hpp"

using namespace et;

int main() {
  using namespace et::pat;
  {
    auto [x] = Vars<double,1>();
    auto e = sin(x)*sin(x) + cos(x)*cos(x);
    RGraph g = compile_to_runtime(e);
    RGraph gn = normalize(g);

    // Pattern: sin(P1)*sin(P1) + cos(P1)*cos(P1)
    Pattern p = (sin(P(1))*sin(P(1))) + (cos(P(1))*cos(P(1)));
    Bindings b;
    bool ok = match(gn, p, b);
    assert(ok);
    assert(b.count(1) == 1);
    int nid = b[1];
    const RNode& bn = gn.nodes[nid];
    assert(bn.kind == NodeKind::Var && bn.var_index == 0);
  }

  {
    // Mismatch case: different placeholder targets must fail
    auto [x,y] = Vars<double,2>();
    auto e = sin(x)*sin(x) + cos(y)*cos(y);
    RGraph g = compile_to_runtime(e);
    RGraph gn = normalize(g);
    Pattern p = (sin(P(1))*sin(P(1))) + (cos(P(1))*cos(P(1)));
    Bindings b;
    bool ok = match(gn, p, b);
    assert(!ok);
  }

  {
    // AC matching inside mul: sin(P1)*sin(P1) should fail on sin(x)*sin(y)
    auto [x,y] = Vars<double,2>();
    auto e = sin(x)*sin(y);
    RGraph g = compile_to_runtime(e);
    RGraph gn = normalize(g);
    Pattern p = sin(P(1))*sin(P(1));
    Bindings b;
    bool ok = match(gn, p, b);
    assert(!ok);
  }

  return 0;
}

