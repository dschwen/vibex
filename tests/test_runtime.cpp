#include <cassert>
#include <vector>

#include "et/expr.hpp"
#include "et/runtime_ast.hpp"

using namespace et;

static bool is_kind(const RGraph& g, int id, NodeKind k) { return g.nodes[id].kind == k; }

int main() {
  auto [x,y,z] = Vars<double,3>();
  auto f = sin(x)*y + z*z;

  RGraph g = compile_to_runtime(f);
  assert(g.root >= 0);
  assert(is_kind(g, g.root, NodeKind::Add));
  const RNode& add = g.nodes[g.root];
  assert(add.ch.size() == 2);

  int a = add.ch[0];
  int b = add.ch[1];
  assert(is_kind(g, a, NodeKind::Mul));
  assert(is_kind(g, b, NodeKind::Mul));

  const RNode& mul1 = g.nodes[a];
  const RNode& mul2 = g.nodes[b];
  assert(mul1.ch.size() == 2);
  assert(mul2.ch.size() == 2);

  // Check sin(x) * y shape
  int m1l = mul1.ch[0];
  int m1r = mul1.ch[1];
  assert(is_kind(g, m1l, NodeKind::Sin));
  assert(is_kind(g, m1r, NodeKind::Var));
  assert(g.nodes[m1r].var_index == 1); // y
  assert(g.nodes[g.nodes[m1l].ch[0]].var_index == 0); // x

  // Check z * z
  int m2l = mul2.ch[0];
  int m2r = mul2.ch[1];
  assert(is_kind(g, m2l, NodeKind::Var));
  assert(is_kind(g, m2r, NodeKind::Var));
  assert(g.nodes[m2l].var_index == 2);
  assert(g.nodes[m2r].var_index == 2);

  return 0;
}

