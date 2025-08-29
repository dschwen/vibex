#include <cassert>

#include "et/ast.hpp"
#include "et/compile_ast.hpp"
#include "et/torch_jit_backend.hpp"

using namespace et;

int main() {
#ifdef ET_WITH_TORCH
  // Build a trivial AST and sanity-check the Torch graph contains nodes
  auto x = var(0), y = var(1);
  Expr f = sin(x) + y;

  TorchJITBackend tb(2);
  auto out = compile_runtime(f, tb);
  (void)out;
  // Graph should have at least one node (sin/add) after compilation
  auto begin = tb.g.nodes().begin();
  auto end = tb.g.nodes().end();
  assert(begin != end);
#endif
  return 0;
}
