#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/compile_runtime.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  // Compare symbolic AD vs tape VJP for Fibonacci a_N wrt (a0,b0)
  for (int k = 0; k <= 7; ++k) {
    auto a0 = Var<double,0>{};
    auto b0 = Var<double,1>{};
    auto n  = lit(static_cast<double>(k));
    auto core = LoopFor<2>(n, a0, b0, State<1>(), State<0>() + State<1>());
    auto aN = Out<0>(core);

    // Symbolic
    auto da_da0 = diff(aN, a0);
    auto da_db0 = diff(aN, b0);
    auto g_da0 = compile_to_runtime(da_da0);
    auto g_db0 = compile_to_runtime(da_db0);
    double sym_da0 = eval(g_da0, {1.23, -0.7});
    double sym_db0 = eval(g_db0, {1.23, -0.7});

    // Tape
    auto g = compile_to_runtime(aN);
    TapeBackend tb(2);
    int root = compile_runtime(g, tb);
    tb.tape.output_id = root;
    auto grad = tb.tape.backward({1.23, -0.7});
    double tape_da0 = grad[0];
    double tape_db0 = grad[1];

    assert(std::abs(sym_da0 - tape_da0) < 1e-12);
    assert(std::abs(sym_db0 - tape_db0) < 1e-12);
  }
  return 0;
}

