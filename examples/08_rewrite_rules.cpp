#include <iostream>
#include <vector>

#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/normalize.hpp"
#include "et/pattern.hpp"
#include "et/match.hpp"
#include "et/rewrite.hpp"
#include "et/rules_default.hpp"
#include "et/compile_runtime.hpp"
#include "et/tape_backend.hpp"
#ifdef ET_WITH_TORCH
#include "et/torch_jit_backend.hpp"
#endif

int main() {
  using namespace et;
  auto [x] = Vars<double,1>();
  auto e = sin(x)*sin(x) + cos(x)*cos(x) + (lit(2.0)*x + lit(3.0)*x);

  // 1) Build runtime graph (keep pre-normalized shape to allow subpattern matches)
  RGraph g = compile_to_runtime(e);

  // 2) Apply algebraic rewrite rules
  auto rules = default_rules();
  RGraph gr = rewrite_fixed_point(g, rules, 6);
  gr = normalize(gr);

  // 3) Evaluate numerically via runtime eval
  double v_eval = eval(gr, std::vector<double>{1.23});

  // 4) Compile to Tape and evaluate forward/backward
  TapeBackend TB(1);
  auto root = compile_runtime(gr, TB);
  TB.tape.output_id = root;
  std::vector<double> in = {1.23};
  double v_tape = TB.tape.forward(in);
  auto grad = TB.tape.backward(in);

  std::cout << "Value (eval)  = " << v_eval << "\n";
  std::cout << "Value (tape)  = " << v_tape << "\n";
  std::cout << "Grad (tape)   = [" << grad[0] << "]\n";

#ifdef ET_WITH_TORCH
  // 5) Compile to Torch JIT graph (optional)
  TorchJITBackend JB(1);
  auto jout = compile_runtime(gr, JB);
  JB.g.registerOutput(jout);
  std::cout << "Torch Graph after rewrites:\n";
  JB.g.print(std::cout);
#endif

  return 0;
}
