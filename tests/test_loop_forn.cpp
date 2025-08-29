#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/ast.hpp"
#include "et/print.hpp"
#include "et/compile.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  // ForN: sum of i for i in [0, n)
  auto n = var(0);
  Expr core = loop_for(1, n, { lit(0.0) }, { state(0) + iter() });
  Expr loop = loop_out(0, core);

  // Quick structural check
  auto s = to_string_pretty(loop);
  assert(s.find("LoopFor(") != std::string::npos);

  // Numeric checks
  TapeBackend tb(1);
  int out = compile_runtime(loop, tb);
  tb.tape.output_id = out;
  for (int k = 0; k <= 12; ++k) {
    double got = tb.tape.forward({static_cast<double>(k)});
    double expected = 0.5 * k * (k - 1);
    assert(std::abs(got - expected) < 1e-9);
  }

  return 0;
}
