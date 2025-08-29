#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/ast.hpp"
#include "et/compile.hpp"
#include "et/torch_jit_backend.hpp"

using namespace et;

int main() {
#ifdef ET_WITH_TORCH
  auto x = var(0);
  Expr y = Select(x > lit(0.0), x + lit(1.0), x - lit(1.0));
  TorchJITBackend JB(1);
  (void)compile_runtime(y, JB);
  bool saw_where = false;
  for (auto* n : JB.g.nodes()) if (n->kind() == c10::Symbol::fromQualString("aten::where")) saw_where = true;
  assert(saw_where);
#endif
  return 0;
}
