#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/runtime_ast.hpp"

using namespace et;

int main() {
  // Fibonacci via ForN with two carried states
  auto n = Var<double,0>{};
  auto a0 = lit(0.0), b0 = lit(1.0);
  auto next_a = State<1>();
  auto next_b = State<0>() + State<1>();
  auto core = Apply<LoopForOp<2>, decltype(n), decltype(a0), decltype(b0), decltype(next_a), decltype(next_b)>(n, a0, b0, next_a, next_b);
  auto aN = Apply<LoopOutOp<0>, decltype(core)>(core);
  auto bN = Apply<LoopOutOp<1>, decltype(core)>(core);

  auto ga = compile_to_runtime(aN);
  auto gb = compile_to_runtime(bN);

  // Check first few values
  struct P { int a,b; } expected[] = {
    {0,1}, {1,1}, {1,2}, {2,3}, {3,5}, {5,8}, {8,13}, {13,21}
  };
  for (int k = 0; k < 8; ++k) {
    double a = eval(ga, {static_cast<double>(k)});
    double b = eval(gb, {static_cast<double>(k)});
    assert(std::abs(a - expected[k].a) < 1e-12);
    assert(std::abs(b - expected[k].b) < 1e-12);
  }
  return 0;
}

