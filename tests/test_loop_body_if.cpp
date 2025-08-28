#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  auto n = Var<double,0>{};
  // s_{t+1} = If(t < 3, s_t + 1, s_t - 1), s_0 = 0
  auto body = If(Iter() < lit(3.0), State<0>() + lit(1.0), State<0>() - lit(1.0));
  auto sN = Out<0>(LoopFor<1>(n, lit(0.0), body));

  auto g = compile_to_runtime(sN);
  auto val = [&](int k){ return eval(g, { (double)k }); };
  // Expected: min(n,3) - max(n-3,0)
  auto exp = [&](int k){ return (double)std::min(k,3) - (double)std::max(k-3,0); };
  for (int k=0;k<=8;++k) {
    assert(std::abs(val(k) - exp(k)) < 1e-12);
  }

  // Tape gradient wrt init state is 1 for any n
  // Build version with variable init: s0 = v
  auto v = Var<double,1>{};
  auto sN_v = Out<0>(LoopFor<1>(n, v, body));
  TapeBackend tb(2);
  auto root = compile(sN_v, tb);
  tb.tape.output_id = root;
  for (int k=0;k<=8;++k) {
    auto grad = tb.tape.backward({(double)k, 0.0});
    assert(std::abs(grad[1] - 1.0) < 1e-12);
  }
  return 0;
}

