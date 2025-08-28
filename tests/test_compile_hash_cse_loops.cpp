#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/compile_hash_cse.hpp"
#include "et/tape_backend.hpp"
#include "et/runtime_ast.hpp"

using namespace et;

int main() {
  // Fibonacci aN via compile_hash_cse → Tape
  auto n = Var<double,0>{};
  auto core = LoopFor<2>(n, lit(0.0), lit(1.0), State<1>(), State<0>() + State<1>());
  auto aN = Out<0>(core);

  // Expected values via runtime evaluator
  auto g = compile_to_runtime(aN);

  TapeBackend tb(1);
  auto root = compile_hash_cse(aN, tb);
  tb.tape.output_id = root;

  for (int k = 0; k <= 10; ++k) {
    double exp = eval(g, {(double)k});
    double got = tb.tape.forward({(double)k});
    assert(std::abs(exp - got) < 1e-12);
  }
  return 0;
}

