#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/runtime_ast.hpp"

using namespace et;

int main() {
  // 1) Running sum with external x: s_{t+1} = s_t + t + x
  auto n = Var<double,0>{};
  auto x = Var<double,1>{};
  auto sum = Out<0>( LoopFor<1>(n, lit(0.0), State<0>() + Iter() + x) );
  auto dsum_dx = diff(sum, x);
  auto g = compile_to_runtime(dsum_dx);
  for (int k = 0; k <= 6; ++k) {
    double got = eval(g, {static_cast<double>(k), 42.0});
    assert(std::abs(got - k) < 1e-12);
  }

  // 2) Fibonacci gradients wrt a0,b0 with n=3: a3 = a0 + 2*b0
  auto a0 = Var<double,0>{};
  auto b0 = Var<double,1>{};
  auto n3 = lit(3.0);
  auto core = LoopFor<2>(n3, a0, b0, State<1>(), State<0>() + State<1>());
  auto aN = Out<0>(core);
  auto da_da0 = diff(aN, a0);
  auto da_db0 = diff(aN, b0);
  auto ga = compile_to_runtime(da_da0);
  auto gb = compile_to_runtime(da_db0);
  // Any inputs for a0,b0 should give 1 and 2
  assert(std::abs(eval(ga, {1.0, 5.0}) - 1.0) < 1e-12);
  assert(std::abs(eval(gb, {1.0, 5.0}) - 2.0) < 1e-12);

  return 0;
}

