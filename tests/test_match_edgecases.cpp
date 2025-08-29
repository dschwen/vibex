#include <cassert>
#include <vector>

#include "et/ast.hpp"
#include "et/ast_to_runtime.hpp"
#include "et/normalize.hpp"
#include "et/match.hpp"
#include "et/pattern.hpp"

using namespace et;

int main() {
  using namespace et::pat;
  auto x = var(0), y = var(1);

  // 1) Spread outside AC node used twice in same pattern: add(neg(S(1)), neg(S(1)))
  //    This forces the spread to bind to the same single child on both occurrences.
  //    Using different children should fail.
  {
    Expr e = (-sin(x)) + (-cos(x));
    RGraph g = normalize(ast_to_rgraph(e));
    Bindings b; MultiBindings mb;
    bool ok = match(g, add(neg(S(1)), neg(S(1))), b, mb);
    assert(!ok);
  }

  // 2) Conflicting placeholder bindings under AC: Add(P1,P1) vs Add(x,y) (x!=y) -> fail
  {
    Expr e = x + y;
    RGraph g = normalize(ast_to_rgraph(e));
    Bindings b; MultiBindings mb;
    bool ok = match(g, add(P(1), P(1)), b, mb);
    assert(!ok);
  }

  // 3) AC mismatch without spread: Add(P1,P2) vs Add(a,b,c) -> fail
  {
    Expr e = x + y + lit(1.0);
    RGraph g = normalize(ast_to_rgraph(e));
    Bindings b; MultiBindings mb;
    bool ok = match(g, add(P(1), P(2)), b, mb);
    assert(!ok);
  }

  // 4) AC with one spread: Add(P1, S(2)) vs Add(a,b,c) -> succeed, spread captures remainder
  {
    Expr e = x + y + lit(1.0);
    RGraph g = normalize(ast_to_rgraph(e));
    Bindings b; MultiBindings mb;
    bool ok = match(g, pat::Pattern::node(NodeKind::Add, { P(1), S(2) }), b, mb);
    assert(ok);
    auto it = mb.find(2); assert(it != mb.end());
    // remainder should be size 2
    assert(it->second.size() == 2);
  }

  return 0;
}
