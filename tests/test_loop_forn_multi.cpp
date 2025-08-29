#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/ast.hpp"
#include "et/compile.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  // Fibonacci via AST loop with two carried states
  auto n = var(0);
  Expr core = loop_for(2, n, { lit(0.0), lit(1.0) }, { state(1), state(0) + state(1) });
  Expr aN = loop_out(0, core);
  Expr bN = loop_out(1, core);

  TapeBackend tbA(1), tbB(1);
  int outA = compile_runtime(aN, tbA); tbA.tape.output_id = outA;
  int outB = compile_runtime(bN, tbB); tbB.tape.output_id = outB;

  struct P { int a,b; } expected[] = {
    {0,1}, {1,1}, {1,2}, {2,3}, {3,5}, {5,8}, {8,13}, {13,21}
  };
  for (int k = 0; k < 8; ++k) {
    double a = tbA.tape.forward({static_cast<double>(k)});
    double b = tbB.tape.forward({static_cast<double>(k)});
    assert(std::abs(a - expected[k].a) < 1e-12);
    assert(std::abs(b - expected[k].b) < 1e-12);
  }
  return 0;
}
