#include <cassert>
#include <cmath>
#include <vector>
#include "et/ast.hpp"
#include "et/compile_hash_cse_ast.hpp"
#include "et/tape_backend.hpp"

using namespace et;

static double fibn(int n) {
  if (n <= 0) return 0.0;
  double a = 0.0, b = 1.0;
  for (int i = 0; i < n; ++i) { double next = b; b = a + b; a = next; }
  return a;
}

int main() {
  // Fibonacci aN via AST loop + hash CSE → Tape
  auto n = var(0);
  Expr core = loop_for(2, n, { lit(0.0), lit(1.0) }, { state(1), state(0) + state(1) });
  Expr aN = loop_out(0, core);

  TapeBackend tb(1);
  auto root = compile_hash_cse_ast(aN, tb);
  tb.tape.output_id = root;

  for (int k = 0; k <= 10; ++k) {
    double exp = fibn(k);
    double got = tb.tape.forward({(double)k});
    assert(std::abs(exp - got) < 1e-12);
  }
  return 0;
}
