#include <iostream>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/ast.hpp"
#include "et/compile.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  // ForN: sum of i for i in [0, n)
  auto n = var(0);
  Expr core = loop_for(1, n, { lit(0.0) }, { state(0) + iter() });
  Expr sum = loop_out(0, core);

  TapeBackend tb(1);
  int out = compile_runtime(sum, tb);
  tb.tape.output_id = out;
  for (int k : {0,1,2,5,10}) {
    double res = tb.tape.forward({static_cast<double>(k)});
    double expected = 0.5 * k * (k - 1);
    std::cout << "n=" << k << " -> " << res << " (expected " << expected << ")\n";
  }
  return 0;
}
