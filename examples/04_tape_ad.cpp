#include <iostream>
#include <vector>
#include "et/expr.hpp"
#include "et/tape_backend.hpp"

int main() {
  using namespace et;
  auto [x,y,z] = Vars<double,3>();
  auto f = sin(x)*y + z*z;

  TapeBackend TB(3);
  int out_id = compile(f, TB);
  TB.tape.output_id = out_id;

  std::vector<double> inputs = {2.4, 6.0, 1.5};
  double v = TB.tape.forward(inputs);
  auto g = TB.tape.vjp(inputs);

  std::cout << "f(2.4,6,1.5) = " << v << "\n";
  std::cout << "grad = [" << g[0] << ", " << g[1] << ", " << g[2] << "]\n";
  return 0;
}
