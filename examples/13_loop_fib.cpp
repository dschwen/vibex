#include <iostream>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/torch_jit_backend.hpp"

using namespace et;

int main() {
  // Fibonacci via ForN with two carried states: (a,b) <- (b, a+b)
  auto n = Var<double,0>{};
  auto a0 = lit(0.0);
  auto b0 = lit(1.0);
  auto next_a = State<1>();
  auto next_b = State<0>() + State<1>();
  // Build core and select outputs a_N and b_N via helpers
  auto core = LoopFor<2>(n, a0, b0, next_a, next_b);
  auto aN = Out<0>(core);
  auto bN = Out<1>(core);

  // Evaluate runtime for a few n
  auto g_a = compile_to_runtime(aN);
  auto g_b = compile_to_runtime(bN);
  std::cout << "AST a_N: " << r_to_string(g_a) << "\n";
  std::cout << "AST b_N: " << r_to_string(g_b) << "\n";
  for (int k : {0,1,2,3,4,5,6,7,8}) {
    double a = eval(g_a, {static_cast<double>(k)});
    double b = eval(g_b, {static_cast<double>(k)});
    std::cout << "n=" << k << " -> (a,b)=(" << a << "," << b << ")\n";
  }

#ifdef ET_WITH_TORCH
  // Lower to Torch graphs for a_N and its symbolic gradients
  TorchJITBackend JB(1);
  auto aN_v = compile(aN, JB);
  JB.g.registerOutput(aN_v);
  std::cout << "Torch graph for fib a_N:\n";
  std::cout << JB.g.toString() << "\n";

  // Symbolic gradients wrt a0 and b0
  auto da_da0 = diff(aN, a0);
  auto da_db0 = diff(aN, b0);
  TorchJITBackend JB2(1);
  auto ga0_v = compile(da_da0, JB2);
  auto gb0_v = compile(da_db0, JB2);
  JB2.g.registerOutput(ga0_v);
  JB2.g.registerOutput(gb0_v);
  std::cout << "Torch graph for d a_N / d(a0,b0):\n";
  std::cout << JB2.g.toString() << "\n";
#endif
  return 0;
}
