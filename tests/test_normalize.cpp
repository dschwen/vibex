#include <cassert>
#include <vector>

#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/normalize.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-12) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

int main() {
  {
    // Flatten and sort Add; fold constants; drop zeros
    auto [x,y,z] = Vars<double,3>();
    auto e = ((x + (y + z)) + lit(0.0)) + (lit(2.0) + lit(3.0));
    RGraph g = compile_to_runtime(e);
    RGraph gn = normalize(g);
    const RNode& root = gn.nodes[gn.root];
    assert(root.kind == NodeKind::Add);
    // Expect 4 children: x, y, z, const(5)
    assert(root.ch.size() == 4);
    bool have_x=false, have_y=false, have_z=false, have_c=false; double cval=0;
    for (int cid : root.ch) {
      const RNode& n = gn.nodes[cid];
      if (n.kind == NodeKind::Var) {
        if (n.var_index == 0) have_x=true; else if (n.var_index == 1) have_y=true; else if (n.var_index == 2) have_z=true;
      } else if (n.kind == NodeKind::Const) { have_c=true; cval = n.cval; }
    }
    assert(have_x && have_y && have_z && have_c);
    assert(approx(cval, 5.0));
  }

  {
    // Flatten and sort Mul; drop ones; annihilator zero
    auto [x,y] = Vars<double,2>();
    auto e = (x * (lit(1.0) * y)) * lit(1.0);
    RGraph g = compile_to_runtime(e);
    RGraph gn = normalize(g);
    const RNode& root = gn.nodes[gn.root];
    assert(root.kind == NodeKind::Mul);
    // Expect exactly two children: x and y (any order but deterministic)
    assert(root.ch.size() == 2);
    int a = root.ch[0], b = root.ch[1];
    assert(gn.nodes[a].kind == NodeKind::Var);
    assert(gn.nodes[b].kind == NodeKind::Var);
    // Annihilator test: (x * 0 * y) -> 0
    auto e0 = x * lit(0.0) * y;
    RGraph g0 = compile_to_runtime(e0);
    RGraph g0n = normalize(g0);
    const RNode& r0 = g0n.nodes[g0n.root];
    assert(r0.kind == NodeKind::Const && r0.cval == 0.0);
  }

  {
    // Simple neutral rules for Sub/Div
    auto [x] = Vars<double,1>();
    {
      RGraph g = compile_to_runtime(x - lit(0.0));
      RGraph gn = normalize(g);
      // Normalize should reduce to x directly
      const RNode& n = gn.nodes[gn.root];
      assert(n.kind == NodeKind::Var && n.var_index == 0);
    }
    {
      RGraph g = compile_to_runtime(lit(0.0) / x);
      RGraph gn = normalize(g);
      const RNode& n = gn.nodes[gn.root];
      assert(n.kind == NodeKind::Const && n.cval == 0.0);
    }
    {
      RGraph g = compile_to_runtime(x / lit(1.0));
      RGraph gn = normalize(g);
      const RNode& n = gn.nodes[gn.root];
      assert(n.kind == NodeKind::Var && n.var_index == 0);
    }
  }

  return 0;
}

