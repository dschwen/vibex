#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/ast.hpp"
#include "et/compile.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  // Fib loop with two carried states; check d aN / d(a0,b0)
  Expr a0 = var(0);
  Expr b0 = var(1);
  Expr n  = lit(3.0); // three steps: a3 = a0 + 2*b0
  Expr core = loop_for(2, n, { a0, b0 }, { state(1), state(0) + state(1) });
  Expr aN = loop_out(0, core);
  TapeBackend tb(2);
  auto root = compile_runtime(aN, tb);
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
