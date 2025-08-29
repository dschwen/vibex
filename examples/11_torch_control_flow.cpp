#include <iostream>

#define ET_ENABLE_CONTROL_FLOW 1
#include "et/ast.hpp"
#include "et/compile_ast.hpp"
#include "et/torch_jit_backend.hpp"

int main() {
  using namespace et;
#ifdef ET_WITH_TORCH
  auto x = var(0);
  Expr expr = Select(x > lit(0.0), x + lit(1.0), x - lit(1.0));
  TorchJITBackend JB(1);
  auto out = compile_runtime(expr, JB);
  JB.g.registerOutput(out);
  std::cout << "Torch JIT graph for Select(x>0, x+1, x-1):\n";
  std::cout << JB.g.toString() << "\n";
#else
  std::cout << "Built without Torch.\n";
#endif
  return 0;
}
