#include <cassert>

#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/torch_jit_backend.hpp"

using namespace et;

int main() {
#ifdef ET_WITH_TORCH
  // Build Select with a comparison mask: should lower to aten::gt + aten::where
  auto [x] = Vars<double,1>();
  auto mask = x > lit(0.0);
  auto expr = Select(mask, x + lit(1.0), x - lit(1.0));

  TorchJITBackend tb1(1);
  (void)compile(expr, tb1);

  bool saw_where = false, saw_gt = false;
  for (auto* n : tb1.g.nodes()) {
    auto k = n->kind();
    if (k == c10::Symbol::fromQualString("aten::where")) saw_where = true;
    if (k == c10::Symbol::fromQualString("aten::gt")) saw_gt = true;
  }
  assert(saw_where && saw_gt);

  // Build logical not of equality: should lower to aten::eq + aten::logical_not
  auto neq = !(x == lit(0.0));
  TorchJITBackend tb2(1);
  (void)compile(neq, tb2);
  bool saw_eq = false, saw_not = false;
  for (auto* n : tb2.g.nodes()) {
    auto k = n->kind();
    if (k == c10::Symbol::fromQualString("aten::eq")) saw_eq = true;
    if (k == c10::Symbol::fromQualString("aten::logical_not")) saw_not = true;
  }
  assert(saw_eq && saw_not);
#endif
  return 0;
}

