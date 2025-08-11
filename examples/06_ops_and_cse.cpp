#include <iostream>
#include "et/expr.hpp"
#include "et/simplify.hpp"
#include "et/tape_backend.hpp"
#include "et/compile_cse.hpp"
#ifdef ET_WITH_TORCH
#include "et/torch_jit_backend.hpp"
#endif

int main() {
  using namespace et;
  auto [x,y,z] = Vars<double,3>();

  auto f = exp(x)*tanh(y) + log(z) + exp(x)*tanh(y) + sqrt(z*z);

  auto dfdx = simplify( diff(f, x) );

  TapeBackend TB(3);
  int out_id = compile_cse(f, TB);
  TB.tape.output_id = out_id;

  std::vector<double> in = {1.2, 0.5, 3.0};
  double v = TB.tape.forward(in);
  auto g = TB.tape.backward(in);

  std::cout << "f(1.2,0.5,3) = " << v << "\n";
  std::cout << "grad = [" << g[0] << ", " << g[1] << ", " << g[2] << "]\n";

#ifdef ET_WITH_TORCH
  TorchJITBackend JB(3);
  auto jout = compile_cse(f, JB);
  JB.g.registerOutput(jout);
  std::cout << "Torch Graph with CSE:\n";
  JB.g.print(std::cout);
#endif
  return 0;
}
