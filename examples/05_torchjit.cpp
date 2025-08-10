#include <iostream>
#include "et/expr.hpp"
#include "et/torch_jit_backend.hpp"

int main() {
#ifdef ET_WITH_TORCH
  using namespace et;
  auto [x,y,z] = Vars<double,3>();
  auto f = sin(x)*y + z*z;
  TorchJITBackend JB(3);
  auto out = compile(f, JB);
  JB.g.registerOutput(out);
  std::cout << "Torch JIT graph:\n";
  JB.g.print(std::cout);
#else
  std::cout << "Rebuild with -DET_WITH_TORCH and link libtorch to run this example.\n";
#endif
  return 0;
}
