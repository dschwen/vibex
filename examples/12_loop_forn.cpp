#include <iostream>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/runtime_ast.hpp"

using namespace et;

int main() {
  // ForN: sum of i for i in [0, n)
  auto n = Var<double,0>{};
  auto init = lit(0.0);
  auto next = State<0>() + Iter();
  auto loop = LoopForN(n, init, next);

  auto g = compile_to_runtime(loop);
  std::cout << "Runtime AST: " << r_to_string(g) << "\n";
  for (int k : {0,1,2,5,10}) {
    double res = eval(g, {static_cast<double>(k)});
    double expected = 0.5 * k * (k - 1);
    std::cout << "n=" << k << " -> " << res << " (expected " << expected << ")\n";
  }
  return 0;
}

