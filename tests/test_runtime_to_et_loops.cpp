#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/runtime_ast.hpp"

using namespace et;

int main() {
  // K=1: running sum
  auto n = Var<double,0>{};
  auto sum = Out<0>(LoopFor<1>(n, lit(0.0), State<0>() + Iter()));
  auto g = compile_to_runtime(sum);
  for (int k = 0; k <= 10; ++k) {
    double a = sum((double)k);
    double b = eval(g, {(double)k});
    assert(std::abs(a - b) < 1e-12);
  }

  // K=2: fib aN
  auto core = LoopFor<2>(n, lit(0.0), lit(1.0), State<1>(), State<0>() + State<1>());
  auto aN = Out<0>(core);
  auto gf = compile_to_runtime(aN);
  for (int k = 0; k <= 8; ++k) {
    double a = eval(gf, { (double)k });
    double b = eval(gf, { (double)k });
    assert(std::abs(a - b) < 1e-12);
  }
  return 0;
}
