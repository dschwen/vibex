#include <cassert>
#include <vector>

#include "et/runtime_ast.hpp"
#include "et/compile_runtime.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  // Manually build a small runtime graph: Add(Var(9), Var(12))
  RGraph g;
  RNode v9; v9.kind = NodeKind::Var; v9.var_index = 9; int id9 = g.add(std::move(v9));
  RNode v12; v12.kind = NodeKind::Var; v12.var_index = 12; int id12 = g.add(std::move(v12));
  RNode add; add.kind = NodeKind::Add; add.ch = {id9, id12}; int root = g.add(std::move(add));
  g.root = root;

  TapeBackend tb(2);
  int out = compile_runtime(g, tb);
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
