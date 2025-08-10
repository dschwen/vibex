#include <cassert>

#include "et/expr.hpp"
#include "et/torch_jit_backend.hpp"

using namespace et;

int main() {
#ifdef ET_WITH_TORCH
  // Build a trivial graph and sanity-check it contains nodes
  auto [x, y] = Vars<double, 2>();
  auto f = sin(x) + y;

  TorchJITBackend tb(2);
  auto out = compile(f, tb);
  (void)out;
  // Graph should have at least one node (sin/add) after compilation
  auto begin = tb.g.nodes().begin();
  auto end = tb.g.nodes().end();
  assert(begin != end);
#endif
  return 0;
}

