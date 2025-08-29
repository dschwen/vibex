// AST shape/properties test (migrated from runtime graph shape test)
#include <cassert>
#include <vector>
#include <memory>
#include "et/ast.hpp"

using namespace et;

int main() {
  auto x = var(0), y = var(1), z = var(2);
  Expr f = sin(x) * y + z * z;

  // Check AST root is Add and children shapes
  auto add = std::dynamic_pointer_cast<AddNode>(f.n);
  assert(add);
  auto mul1 = std::dynamic_pointer_cast<MulNode>(add->a);
  auto mul2 = std::dynamic_pointer_cast<MulNode>(add->b);
  assert(mul1 && mul2);

  // sin(x) * y
  auto sinx = std::dynamic_pointer_cast<SinNode>(mul1->a);
  auto vy   = std::dynamic_pointer_cast<VarNode>(mul1->b);
  assert(sinx && vy && vy->index == 1);
  auto vx_in_sin = std::dynamic_pointer_cast<VarNode>(sinx->a);
  assert(vx_in_sin && vx_in_sin->index == 0);

  // z * z
  auto vz1 = std::dynamic_pointer_cast<VarNode>(mul2->a);
  auto vz2 = std::dynamic_pointer_cast<VarNode>(mul2->b);
  assert(vz1 && vz2 && vz1->index == 2 && vz2->index == 2);

  return 0;
}
