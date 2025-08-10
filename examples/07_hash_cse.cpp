#include <iostream>
#include "et/expr.hpp"
#include "et/simplify.hpp"
#include "et/tape_backend.hpp"
#include "et/compile_hash_cse.hpp"

int main() {
  using namespace et;
  auto [x,y,z] = Vars<double,3>();

  auto g = exp(x) * tanh(y);
  auto f = g + log(z) + g + sqrt(z*z) + g;

  TapeBackend TB(3);
  int out_id = compile_hash_cse(f, TB);
  TB.tape.output_id = out_id;

  std::vector<double> in = {1.1, 0.7, 2.5};
  double v = TB.tape.forward(in);
  auto grad = TB.tape.vjp(in);

  std::cout << "f(1.1,0.7,2.5) = " << v << "\n";
  std::cout << "grad = [" << grad[0] << ", " << grad[1] << ", " << grad[2] << "]\n";
  return 0;
}
