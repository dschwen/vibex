#include <cassert>
#include <iostream>

#define ET_ENABLE_CONTROL_FLOW 1
#include "et/ast.hpp"
#include "et/compile.hpp"
#include "et/tape_backend.hpp"

int main() {
  using namespace et;
  auto x = var(0);

  // If: choose branch based on x > 0
  Expr y = If(x > lit(0.0), lit(1.0) + x, lit(-1.0) + x);

  TapeBackend tb1(1);
  int out1 = compile_runtime(y, tb1);
  tb1.tape.output_id = out1;
  // x = 2 => then branch: 1 + 2 = 3
  assert(tb1.tape.forward({2.0}) == 3.0);
  // x = -5 => else branch: -1 + (-5) = -6
  assert(tb1.tape.forward({-5.0}) == -6.0);

  // Select: elementwise style scalar mask
  Expr z = Select(lit(0.0), lit(10.0), lit(20.0));
  TapeBackend tb2(0);
  int out2 = compile_runtime(z, tb2);
  tb2.tape.output_id = out2;
  assert(tb2.tape.forward({}) == 20.0);

  std::cout << "OK\n";
  return 0;
}
