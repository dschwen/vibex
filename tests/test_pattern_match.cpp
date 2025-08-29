#include <cassert>
#include <unordered_map>

#include "et/ast.hpp"
#include "et/normalize.hpp"
#include "et/pattern.hpp"
#include "et/match.hpp"

using namespace et;

int main() {
  using namespace et::astpat;
  {
    auto x = var(0);
    Expr e = sin(x)*sin(x) + cos(x)*cos(x);
    // Pattern: sin(P1)*sin(P1) + cos(P1)*cos(P1)
    Pattern p = (sin(P(1))*sin(P(1))) + (cos(P(1))*cos(P(1)));
    AstBindings b; AstMultiBindings mb;
    bool ok = match(e, p, b, mb);
    assert(ok);
    assert(b.count(1) == 1);
    Expr cap = b[1];
    auto v = std::dynamic_pointer_cast<VarNode>(cap.n);
    assert(v && v->index == 0);
  }

  {
    // Mismatch case: different placeholder targets must fail
    auto x = var(0), y = var(1);
    Expr e = sin(x)*sin(x) + cos(y)*cos(y);
    Pattern p = (sin(P(1))*sin(P(1))) + (cos(P(1))*cos(P(1)));
    AstBindings b; AstMultiBindings mb;
    bool ok = match(e, p, b, mb);
    assert(!ok);
  }

  {
    // AC matching inside mul: sin(P1)*sin(P1) should fail on sin(x)*sin(y)
    auto x = var(0), y = var(1);
    Expr e = sin(x)*sin(y);
    Pattern p = sin(P(1))*sin(P(1));
    AstBindings b; AstMultiBindings mb;
    bool ok = match(e, p, b, mb);
    assert(!ok);
  }

  return 0;
}
