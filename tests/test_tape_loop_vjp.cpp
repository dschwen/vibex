#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/compile_runtime.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  // Fib loop with two carried states; check d aN / d(a0,b0)
  auto a0 = Var<double,0>{};
  auto b0 = Var<double,1>{};
  auto n  = lit(3.0); // three steps: a3 = a0 + 2*b0
  auto next_a = State<1>();
  auto next_b = State<0>() + State<1>();
  auto core = LoopFor<2>(n, a0, b0, next_a, next_b);
  auto aN = Out<0>(core);

  // Compile to runtime graph, then to tape
  auto g = compile_to_runtime(aN);
  TapeBackend tb(2);
  auto root = compile_runtime(g, tb);
  tb.tape.output_id = root;

  // Backward wrt (a0,b0)
  std::vector<double> in = {1.0, 1.0};
  auto grad = tb.tape.backward(in);
  assert(grad.size() >= 2);
  // daN/da0 = 1; daN/db0 = 2
  assert(std::abs(grad[0] - 1.0) < 1e-12);
  assert(std::abs(grad[1] - 2.0) < 1e-12);
  return 0;
}

