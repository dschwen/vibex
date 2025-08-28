#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/compile_runtime.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  // compile_runtime does not lower loops; fallback should return zero
  auto n = Var<double,0>{};
  auto sum = Out<0>(LoopFor<1>(n, lit(0.0), State<0>() + Iter()));
  auto g = compile_to_runtime(sum);
  TapeBackend tb(1);
  auto out = compile_runtime(g, tb);
  tb.tape.output_id = out;
  for (int k = 0; k <= 3; ++k) {
    double got = tb.tape.forward({(double)k});
    assert(got == 0.0);
  }
  return 0;
}

