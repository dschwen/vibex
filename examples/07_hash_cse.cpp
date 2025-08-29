#include <iostream>
#include <vector>
#include "et/ast.hpp"
#include "et/compile_hash_cse.hpp"
#include "et/tape_backend.hpp"

int main() {
  using namespace et;
  auto x = var(0), y = var(1), z = var(2);

  Expr g = exp(x) * tanh(y);
  Expr f = g + log(z) + g + sqrt(z*z) + g;

  TapeBackend TB(3);
  int out_id = compile_hash_cse(f, TB);
  TB.tape.output_id = out_id;

  std::vector<double> in = {1.1, 0.7, 2.5};
  double v = TB.tape.forward(in);
  auto grad = TB.tape.backward(in);

  std::cout << "f(1.1,0.7,2.5) = " << v << "\n";
  std::cout << "grad = [" << grad[0] << ", " << grad[1] << ", " << grad[2] << "]\n";
  return 0;
}
