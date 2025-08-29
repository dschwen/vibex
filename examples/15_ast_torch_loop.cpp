#include <iostream>
#include <vector>
#include "et/ast.hpp"
#include "et/compile_cse_ast.hpp"
#include "et/torch_jit_backend.hpp"

using namespace et;

int main() {
#ifndef ET_WITH_TORCH
  std::cout << "Build with -DET_WITH_TORCH=ON to run this example.\n";
  return 0;
#else
  // Fibonacci via AST loop: carry (a,b) <- (b, a+b)
  auto n = var(0);
  Expr core = loop_for(2, n, { lit(0.0), lit(1.0) }, { state(1), state(0) + state(1) });
  Expr aN = loop_out(0, core);

  // Compile to Torch JIT graph using AST CSE
  TorchJITBackend JB(1);
  auto out = compile_cse(aN, JB);
  JB.g.registerOutput(out);
  std::cout << "Torch graph (prim::Loop) for a_N fib:\n";
  std::cout << JB.g.toString() << "\n";
  return 0;
#endif
}
