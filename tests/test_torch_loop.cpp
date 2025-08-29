#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/ast.hpp"
#include "et/compile_ast.hpp"
#include "et/torch_jit_backend.hpp"

using namespace et;

int main() {
#ifdef ET_WITH_TORCH
  // Fibonacci aN via AST to Torch JIT prim::Loop
  auto n = var(0);
  Expr core = loop_for(2, n, { lit(0.0), lit(1.0) }, { state(1), state(0) + state(1) });
  Expr aN = loop_out(0, core);
  TorchJITBackend JB(1);
  (void)compile_runtime(aN, JB);
  bool saw_loop = false;
  for (auto* node : JB.g.nodes()) if (node->kind() == torch::jit::prim::Loop) { saw_loop = true; break; }
  assert(saw_loop);
#endif
  return 0;
}
