#include <iostream>
#include <vector>

#include "et/ast.hpp"
#include "et/normalize.hpp"
#include "et/rewrite_ast.hpp"
#include "et/compile.hpp"
#include "et/tape_backend.hpp"
#ifdef ET_WITH_TORCH
#include "et/torch_jit_backend.hpp"
#endif

int main() {
  using namespace et;
  auto x = var(0);
  Expr e = sin(x)*sin(x) + cos(x)*cos(x) + (lit(2.0)*x + lit(3.0)*x);

  // Rewrite AST to fixed point and evaluate
  Expr r = rewrite_fixed_point(e);
  std::vector<double> in = {1.23};
  double v_eval = eval(r, in);

  // Compile to Tape and evaluate forward/backward
  TapeBackend TB(1);
  auto root = compile_runtime(r, TB);
  TB.tape.output_id = root;
  double v_tape = TB.tape.forward(in);
  auto grad = TB.tape.backward(in);

  std::cout << "Value (eval)  = " << v_eval << "\n";
  std::cout << "Value (tape)  = " << v_tape << "\n";
  std::cout << "Grad (tape)   = [" << grad[0] << "]\n";

#ifdef ET_WITH_TORCH
  // Compile to Torch JIT graph (optional)
  TorchJITBackend JB(1);
  auto jout = compile_runtime(r, JB);
  JB.g.registerOutput(jout);
  std::cout << "Torch Graph after AST rewrites:\n";
  JB.g.print(std::cout);
#endif

  return 0;
}
