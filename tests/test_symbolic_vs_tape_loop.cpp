#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/tape_backend.hpp"
#include "et/compile_runtime.hpp"

using namespace et;

int main() {
  // Running sum with external x: s_{t+1} = s_t + t + x; check d sum/dx = n
  auto n = Var<double,0>{};
  auto x = Var<double,1>{};
  auto sum = Out<0>( LoopFor<1>(n, lit(0.0), State<0>() + Iter() + x) );
  auto dsum_dx = diff(sum, x);

  // Runtime graph for symbolic diff
  auto g_sym = compile_to_runtime(dsum_dx);

  // Tape for primal to compare backward
  auto g_pr = compile_to_runtime(sum);
  TapeBackend tb(2);
  auto root = compile_runtime(g_pr, tb);
  tb.tape.output_id = root;

  for (int k = 0; k <= 8; ++k) {
    double sym = eval(g_sym, {static_cast<double>(k), 3.14});
    auto grad = tb.tape.backward({static_cast<double>(k), 3.14});
    double tape = grad[1]; // wrt x
    assert(std::abs(sym - k) < 1e-12);
    assert(std::abs(tape - k) < 1e-12);
  }
  return 0;
}

