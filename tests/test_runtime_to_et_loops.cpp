#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/ast.hpp"
#include "et/ast_to_runtime.hpp"

using namespace et;

int main() {
  // K=1: running sum
  auto n = et::var(0);
  et::Expr sum = loop_out(0, loop_for(1, n, { lit(0.0) }, { state(0) + iter() }));
  auto g = ast_to_rgraph(sum);
  for (int k = 0; k <= 10; ++k) {
    double a = eval(g, {(double)k});
    double b = eval(g, {(double)k});
    assert(std::abs(a - b) < 1e-12);
  }

  // K=2: fib aN
  et::Expr core = loop_for(2, n, { lit(0.0), lit(1.0) }, { state(1), state(0) + state(1) });
  et::Expr aN = loop_out(0, core);
  auto gf = ast_to_rgraph(aN);
  for (int k = 0; k <= 8; ++k) {
    double a = eval(gf, { (double)k });
    double b = eval(gf, { (double)k });
    assert(std::abs(a - b) < 1e-12);
  }
  return 0;
}
