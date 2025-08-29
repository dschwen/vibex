#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/ast.hpp"
#include "et/compile.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  auto n = var(0);
  // s_{t+1} = If(t < 3, s_t + 1, s_t - 1)
  Expr body = If(iter() < lit(3.0), state(0) + lit(1.0), state(0) - lit(1.0));

  // Fixed init s0=0
  Expr sN = loop_out(0, loop_for(1, n, { lit(0.0) }, { body }));
  TapeBackend tb1(1);
  int out1 = compile_runtime(sN, tb1);
  tb1.tape.output_id = out1;
  auto val = [&](int k){ return tb1.tape.forward({ (double)k }); };
  auto exp = [&](int k){ return (double)std::min(k,3) - (double)std::max(k-3,0); };
  for (int k=0;k<=8;++k) assert(std::abs(val(k) - exp(k)) < 1e-12);

  // Variable init s0=v; gradient wrt v should be 1 for any n
  auto v = var(1);
  Expr sN_v = loop_out(0, loop_for(1, n, { v }, { body }));
  TapeBackend tb2(2);
  int out2 = compile_runtime(sN_v, tb2);
  tb2.tape.output_id = out2;
  for (int k=0;k<=8;++k) {
    auto grad = tb2.tape.backward({(double)k, 0.0});
    assert(std::abs(grad[1] - 1.0) < 1e-12);
  }
  return 0;
}
