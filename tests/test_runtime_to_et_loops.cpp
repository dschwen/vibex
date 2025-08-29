#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/ast.hpp"
#include "et/compile.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  // K=1: running sum
  auto n = et::var(0);
  et::Expr sum = loop_out(0, loop_for(1, n, { lit(0.0) }, { state(0) + iter() }));
  TapeBackend tb1(1);
  int out1 = compile_runtime(sum, tb1);
  tb1.tape.output_id = out1;
  for (int k = 0; k <= 10; ++k) {
    double a = tb1.tape.forward({(double)k});
    double b = tb1.tape.forward({(double)k});
    assert(std::abs(a - b) < 1e-12);
  }

  // K=2: fib aN
  et::Expr core = loop_for(2, n, { lit(0.0), lit(1.0) }, { state(1), state(0) + state(1) });
  et::Expr aN = loop_out(0, core);
  TapeBackend tb2(1);
  int out2 = compile_runtime(aN, tb2);
  tb2.tape.output_id = out2;
  for (int k = 0; k <= 8; ++k) {
    double a = tb2.tape.forward({(double)k});
    double b = tb2.tape.forward({(double)k});
    assert(std::abs(a - b) < 1e-12);
  }
  return 0;
}
