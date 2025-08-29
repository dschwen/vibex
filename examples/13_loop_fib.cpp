#include <iostream>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/ast.hpp"
#include "et/compile_ast.hpp"
#include "et/torch_jit_backend.hpp"

using namespace et;

int main() {
  // Fibonacci via AST LoopFor with two carried states: (a,b) <- (b, a+b)
  auto n = var(0);
  Expr core = loop_for(2, n, { lit(0.0), lit(1.0) }, { state(1), state(0) + state(1) });
  Expr aN = loop_out(0, core);
  Expr bN = loop_out(1, core);

  // Torch JIT graph for a_N
  #ifdef ET_WITH_TORCH
  TorchJITBackend JB(1);
  auto aN_v = compile_runtime(aN, JB);
  JB.g.registerOutput(aN_v);
  std::cout << "Torch graph for fib a_N:\n";
  std::cout << JB.g.toString() << "\n";
  #endif
  return 0;
}
