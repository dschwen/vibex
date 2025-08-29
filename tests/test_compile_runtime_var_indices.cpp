#include <cassert>
#include <vector>

#include "et/ast.hpp"
#include "et/compile_ast.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  // Build a small AST: Add(Var(9), Var(12)) and compile to Tape
  Expr v9 = var(9);
  Expr v12 = var(12);
  Expr e = v9 + v12;

  TapeBackend tb(2);
  int out = compile_runtime(e, tb);
  tb.tape.output_id = out;

  // Expect two Var nodes emitted with indices 9 and 12 (no fallback)
  int seen_vars = 0;
  for (const auto& n : tb.tape.nodes) if (n.kind == Tape::KVar) ++seen_vars;
  assert(seen_vars == 2);

  // Scan order follows DFS; ensure both indices {9,12} are present
  bool has9=false, has12=false;
  for (const auto& n : tb.tape.nodes) if (n.kind == Tape::KVar) {
    if (n.var_index == 9) has9 = true;
    if (n.var_index == 12) has12 = true;
  }
  assert(has9 && has12);
  return 0;
}
