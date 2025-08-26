#include <cassert>
#include <iostream>

#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/runtime_ast.hpp"

int main() {
  using namespace et;
  auto [x] = Vars<double,1>();

  // If: choose branch based on x > 0
  auto y = If(x > lit(0.0), lit(1.0) + x, lit(-1.0) + x);

  auto g = compile_to_runtime(y);
  // x = 2 => then branch: 1 + 2 = 3
  assert(eval(g, {2.0}) == 3.0);
  // x = -5 => else branch: -1 + (-5) = -6
  assert(eval(g, {-5.0}) == -6.0);

  // Select: elementwise style scalar mask
  auto z = Select(lit(0.0), lit(10.0), lit(20.0));
  auto gz = compile_to_runtime(z);
  assert(eval(gz, {}) == 20.0);

  std::cout << "OK\n";
  return 0;
}
