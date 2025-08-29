#include <iostream>
#include <vector>
#include "et/ast.hpp"
#include "et/normalize_ast.hpp"
#include "et/rewrite_ast.hpp"
#include "et/compile_ast.hpp"
#include "et/tape_backend.hpp"

int main() {
  using namespace et;
  auto x = var(0), y = var(1), z = var(2);
  Expr f = sin(x)*y + z*z + lit(0.0);
  // Simplify with AST normalization + rewrite
  Expr fs = rewrite_fixed_point(f);

  TapeBackend tb(3);
  int root = compile_runtime(fs, tb);
  tb.tape.output_id = root;

  std::vector<double> in = {2.4, 6.0, 1.5};
  double val = tb.tape.forward(in);
  auto grad = tb.tape.backward(in);
  std::cout << "f(2.4,6,1.5)  = " << val << "\n";
  std::cout << "df/d(x,y,z) = [" << grad[0] << ", " << grad[1] << ", " << grad[2] << "]\n";
  return 0;
}
